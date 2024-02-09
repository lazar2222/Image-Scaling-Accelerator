library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.components.all;

-- Image up/down scaling accelerator top level
entity acc_scale is
  generic
  (
    max_width : integer := 1024
  );
  port
  (
    clk : in std_logic;
    rst : in std_logic;

    asi_data  : in std_logic_vector(7 downto 0);
    asi_ready : out std_logic;
    asi_valid : in std_logic;
    asi_eop   : in std_logic;
    asi_sop   : in std_logic;

    aso_data  : out std_logic_vector(7 downto 0);
    aso_ready : in std_logic;
    aso_valid : out std_logic;
    aso_eop   : out std_logic;
    aso_sop   : out std_logic;

    amms_address     : in std_logic;
    amms_read        : in std_logic;
    amms_readdata    : out std_logic_vector(31 downto 0);
    amms_write       : in std_logic;
    amms_writedata   : in std_logic_vector(31 downto 0);
    amms_waitrequest : out std_logic
  );
end entity acc_scale;

architecture rtl of acc_scale is
  signal img_height : unsigned(15 downto 0);
  signal img_width  : unsigned(15 downto 0);
  signal y_upscale  : boolean;
  signal y_scale    : unsigned(1 downto 0);
  signal x_upscale  : boolean;
  signal x_scale    : unsigned(1 downto 0);

  constant CSR_ADDR : std_logic := '0';
  constant WHR_ADDR : std_logic := '1';

  signal csr_strobe : boolean;
  signal whr_strobe : boolean;

  signal csr_reg     : std_logic_vector(31 downto 0);
  signal whr_reg     : std_logic_vector(31 downto 0);
  signal readout_mux : std_logic_vector(31 downto 0);

  signal x_scale_actual : unsigned(2 downto 0);
  signal y_scale_actual : unsigned(2 downto 0);

  signal reader_row_ahead      : boolean;
  signal reader_row_equal      : boolean;
  signal reader_row_one_behind : boolean;
  signal reader_row_behind     : boolean;
  signal reader_pixel_behind   : boolean;
  signal reader_pixel_ahead    : boolean;
  signal reader_last_rep       : boolean;
  signal output_can_read       : boolean;
  signal stream_can_write      : boolean;

  signal reset        : boolean;
  signal cnt_reset    : boolean;
  
  signal stream_next  : boolean;
  signal stream_pixel : unsigned(15 downto 0);
  signal stream_row   : unsigned(15 downto 0);
  
  signal output_next    : boolean;
  signal output_pixel   : unsigned(15 downto 0);
  signal output_row     : unsigned(15 downto 0);
  signal output_row_rep : unsigned(2 downto 0);

  signal buffer_in    : unsigned(7 downto 0);
  signal buffer_out   : unsigned(7 downto 0);
begin

  amms_waitrequest <= '0';

  -- Address map
  -- 0 : Control and Status Register
  -- 1 : Width and Height Register
  csr_strobe <= TRUE when (amms_write = '1') and (amms_address = CSR_ADDR) else FALSE;
  whr_strobe <= TRUE when (amms_write = '1') and (amms_address = WHR_ADDR) else FALSE;

  -- Control and Status Register Map
  -- 31..6 : Reserved
  --     5 : Y upscale
  --  4..3 : Y scale
  --     2 : X upscale
  --  1..0 : X scale
  csr_reg <= (31 downto 6 => '0') & stdlogic(y_upscale) & std_logic_vector(y_scale) & stdlogic(x_upscale) & std_logic_vector(x_scale);
  
  -- Width and Height Register Map
  -- 31..16 : Image Height
  -- 15..0  : Image Width
  whr_reg <= std_logic_vector(img_height) & std_logic_vector(img_width);

  readout_mux <= csr_reg when amms_address = CSR_ADDR else whr_reg when amms_address = WHR_ADDR else (31 downto 0 => '0');

  process (clk, rst)
  begin
    if (rst = '1') then
      img_height <= to_unsigned(0, img_height'length);
      img_width  <= to_unsigned(0, img_width'length);
      y_scale    <= to_unsigned(0, y_scale'length);
      x_upscale  <= FALSE;
      x_scale    <= to_unsigned(0, x_scale'length);
    elsif (rising_edge(clk)) then
      if (csr_strobe) then
        y_upscale <= bool(amms_writedata(5));
        y_scale   <= unsigned(amms_writedata(4 downto 3));
        x_upscale <= bool(amms_writedata(2));
        x_scale   <= unsigned(amms_writedata(1 downto 0));
      end if;
      if (whr_strobe) then
        img_height <= unsigned(amms_writedata(31 downto 16));
        img_width  <= unsigned(amms_writedata(15 downto 0));
      end if;
      amms_readdata <= readout_mux;
    end if;
  end process;

  -- Convert scales from encoded reigster format to an actual number
  x_scale_actual <= resize(x_scale, x_scale_actual'length) + to_unsigned(1, x_scale_actual'length);
  y_scale_actual <= resize(y_scale, y_scale_actual'length) + to_unsigned(1, y_scale_actual'length);

  -- Helper signals for keeping track of relative positions of read and write pointers
  reader_row_ahead      <= output_row > stream_row;
  reader_row_equal      <= output_row = stream_row;
  reader_row_one_behind <= output_row = stream_row - to_unsigned(1, stream_row'length);
  reader_row_behind     <= output_row < stream_row;

  reader_pixel_behind <= output_pixel < stream_pixel;
  reader_pixel_ahead  <= output_pixel > stream_pixel;

  reader_last_rep <= output_row_rep = (y_scale_actual - to_unsigned(1, y_scale_actual'length)) or not(y_upscale);

  -- We can output pixel from the buffer only if writer has already written the corresponding pixel to buffer
  output_can_read <= reader_row_behind or (reader_row_equal and reader_pixel_behind);

  -- We can write pixel to buffer only if reader won't need the pixel we are overwriting
  stream_can_write <= reader_row_ahead or reader_row_equal or (reader_row_one_behind and reader_last_rep and reader_pixel_ahead);

  reset     <= bool(rst);

  -- Reset counter once both have run off the end of the frame
  -- Stream counter shouldnt continue counting past the first pixel of img_height row if dma is setup correctly
  cnt_reset <= reset or (stream_row >= img_height and output_row >= img_height);

  asi_ready <= stdlogic(stream_can_write);
  
  aso_data  <= std_logic_vector(buffer_out);
  aso_valid <= stdlogic(output_can_read);
  aso_sop   <= '0';
  aso_eop   <= '0';

  -- When to advance the counters and write to buffer
  stream_next <= stream_can_write and bool(asi_valid);
  output_next <= bool(aso_ready) and output_can_read;

  buffer_in <= unsigned(asi_data);

  -- Keeps track of the position of the next pixel that comes from the input stream
  -- Scales are hardcoded to 1 and not upscaling since source image is not scaled
  stream_counter : image_counter port map
  (
    clk        => clk,
    rst        => cnt_reset,
    next_pixel => stream_next,
    x_upscale  => FALSE,
    x_scale    => to_unsigned(1, x_scale_actual'length),
    y_upscale  => FALSE,
    y_scale    => to_unsigned(1, x_scale_actual'length),
    img_width  => img_width,
    img_height => img_height,
    pixel      => stream_pixel,
    row        => stream_row
  );

  -- Keeps track of the position of the next pixel to write to the input stream
  output_counter : image_counter port map
  (
    clk        => clk,
    rst        => cnt_reset,
    next_pixel => output_next,
    x_upscale  => x_upscale,
    x_scale    => x_scale_actual,
    y_upscale  => y_upscale,
    y_scale    => y_scale_actual,
    img_width  => img_width,
    img_height => img_height,
    pixel      => output_pixel,
    row_rep    => output_row_rep,
    row        => output_row
  );

  line_buff : line_buffer generic map
  (
    max_width => max_width
  )
  port map 
  (
    clk               => clk,
    rst               => reset,
    buffer_in         => buffer_in,
    buffer_out        => buffer_out,
    buffer_write_addr => stream_pixel,
    buffer_read_addr  => output_pixel,
    write_buffer      => stream_next
  );
end architecture rtl;