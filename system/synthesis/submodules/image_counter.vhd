library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Keeps track of current pixel being read to or written to stream
-- Takes care of skipping or repeating pixels as well as moving to next row
entity image_counter is
  port
  (
    clk        : in std_logic;
    rst        : in boolean;
    next_pixel : in boolean;

    x_upscale        : in boolean;
    x_scale          : in unsigned(2 downto 0);
    y_upscale        : in boolean;
    y_scale          : in unsigned(2 downto 0);

    img_width  : in unsigned(15 downto 0);
    img_height : in unsigned(15 downto 0);

    pixel_rep   : out unsigned(2 downto 0);
    pixel       : out unsigned(15 downto 0);
    row_rep     : out unsigned(2 downto 0);
    row         : out unsigned(15 downto 0)
  );
end entity;

architecture rtl of image_counter is
begin
  process (clk, rst)
    variable pixel_rep_var   : unsigned(2 downto 0);
    variable pixel_var       : unsigned(15 downto 0);
    variable row_rep_var     : unsigned(2 downto 0);
    variable row_var         : unsigned(15 downto 0);
  begin
    if (rst) then
      pixel_rep   <= to_unsigned(0, pixel_rep'length);
      pixel       <= to_unsigned(0, pixel'length);
      row_rep     <= to_unsigned(0, row_rep'length);
      row         <= to_unsigned(0, row'length);
    elsif (rising_edge(clk)) then
      if (next_pixel) then
        -- Increment pixel repetition either way, if donwscaling it will be reset
        pixel_rep_var   := pixel_rep + to_unsigned(1, pixel_rep_var'length);
        pixel_var       := pixel;
        row_rep_var     := row_rep;
        row_var         := row;

        if (not(x_upscale)) then
          -- When downscaling reset pixel repetition and increment pixel instead
          pixel_rep_var := to_unsigned(0, pixel_rep_var'length);
          pixel_var     := pixel_var + x_scale;
        elsif (pixel_rep_var = x_scale) then
          -- Last repetition, reset repetition counter and increment pixel counter
          pixel_rep_var := to_unsigned(0, pixel_rep_var'length);
          pixel_var     := pixel_var + to_unsigned(1, pixel_var'length);
        end if;

        if (pixel_var >= img_width) then
          -- Past the end of the line, rest pixel counter and increment row repetition counter
          pixel_var       := to_unsigned(0, pixel_var'length);
          row_rep_var     := row_rep_var + to_unsigned(1, row_rep_var'length);

          if (not(y_upscale)) then
            -- When downscaling immediately reset row repetition counter and increment row counter
            row_rep_var := to_unsigned(0, row_rep_var'length);
            row_var     := row_var + y_scale;
          elsif (row_rep_var = y_scale) then
            -- Last row repetition, reset repetition counter and increment row counter
            row_rep_var := to_unsigned(0, row_rep_var'length);
            row_var     := row_var + to_unsigned(1, row_var'length);
          end if;
        end if;

        -- Reset after the end of frame is handled externally

        pixel_rep   <= pixel_rep_var;
        pixel       <= pixel_var;
        row_rep     <= row_rep_var;
        row         <= row_var;
      end if;
    end if;
  end process;
end architecture;
