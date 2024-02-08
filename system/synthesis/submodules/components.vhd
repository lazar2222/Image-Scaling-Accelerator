library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package components is
  -- Convenience functions for converting to and from boolean
  function stdlogic(L : boolean) return std_logic;
  function bool(L     : std_logic) return boolean;

  component image_counter
    port
    (
      clk        : in std_logic;
      rst        : in boolean;
      next_pixel : in boolean;
      x_upscale  : in boolean;
      x_scale    : in unsigned(2 downto 0);
      y_upscale  : in boolean;
      y_scale    : in unsigned(2 downto 0);
      img_width  : in unsigned(15 downto 0);
      img_height : in unsigned(15 downto 0);
      pixel_rep  : out unsigned(2 downto 0);
      pixel      : out unsigned(15 downto 0);
      row_rep    : out unsigned(2 downto 0);
      row        : out unsigned(15 downto 0)
    );
  end component;

  component line_buffer
    generic
    (
      max_width : integer
    );
    port
    (
      clk               : in std_logic;
      rst               : in boolean;
      buffer_in         : in unsigned(7 downto 0);
      buffer_out        : out unsigned(7 downto 0);
      buffer_write_addr : in unsigned(15 downto 0);
      buffer_read_addr  : in unsigned(15 downto 0);
      write_buffer      : in boolean
    );
  end component;
end components;

package body components is
  -- Convenience functions for converting to and from boolean
  function stdlogic(L : boolean) return std_logic is
  begin
    if L then
      return('1');
    else
      return('0');
    end if;
  end function stdlogic;

  function bool(L : std_logic) return boolean is
  begin
    if L = '1' then
      return(TRUE);
    else
      return(FALSE);
    end if;
  end function bool;
end package body;
