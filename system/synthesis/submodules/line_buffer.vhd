library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.components.all;

-- Dual (one write, one read) port synchronous RAM
-- Should be inferred as altsyncram
entity line_buffer is
  generic
  (
    max_width : integer := 1024
  );
  port
  (
    clk : in std_logic;
    rst : in boolean;

    buffer_in  : in unsigned(7 downto 0);
    buffer_out : out unsigned(7 downto 0);

    buffer_write_addr : in unsigned(15 downto 0);
    buffer_read_addr  : in unsigned(15 downto 0);

    write_buffer : in boolean
  );
end entity;

architecture rtl of line_buffer is
  type buffer_type is array (0 to max_width - 1) of unsigned (7 downto 0);
  signal line_buffer    : buffer_type;
begin
  -- Output is not registered as altsyncram requires registered read address and not output data
  -- Read address register is not implemented here, making it look like an asynchronous RAM
  -- Since it is driven directly by register from image_counter Quartus can infer that it is actually synchronous
  buffer_out <= line_buffer(to_integer(buffer_read_addr));
  process (clk, rst)
  begin
    if (rst) then
    elsif (rising_edge(clk)) then
      if (write_buffer) then
        line_buffer(to_integer(buffer_write_addr)) <= buffer_in;
      end if;
    end if;
  end process;
  end architecture;