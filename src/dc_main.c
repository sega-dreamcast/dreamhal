
#include "dc_main.h"

// main function
int dreamcast_main(void)
{
  // First things first: let's get a more modern color mode up
  // Set up color mode and resolution (cable type and console region are auto-detected by this)
  STARTUP_Init_Video(FB_RGB0888, USE_640x480);
  //  STARTUP_Init_Video(FB_RGB0888, USE_320x240);

//
// Main body start
//

// Random test code

  if(STARTUP_dcload_present == DCLOAD_CONSOLE)
  {
    if(DCLOAD_type == DCLOAD_TYPE_IP)
    {
      dcloadsyscall(DCLOAD_WRITE, 1, "IP: Success!\n", 15);
    }
    else if(DCLOAD_type == DCLOAD_TYPE_SER)
    {
      dcloadsyscall(DCLOAD_WRITE, 1, "Serial: Success!\n", 19);
    }
    else
    {
      dcloadsyscall(DCLOAD_WRITE, 1, "Failure!\n", 11);
    }
  }

  printf("Printf test! 0x%x\r\n", STARTUP_dcload_present);

  char test_array[20] = {0};

  printf("%s\n", hex_to_string(1 << 31, test_array));
  printf("%s\n", hex_to_string(1024, test_array));

  printf("%s\n", uint_to_string(0xffffffff, test_array));
  printf("%s\n", uint_to_string(1 << 31, test_array));
  printf("%s\n", uint_to_string(1024, test_array));

  printf("%s\n", int_to_string(1 << 31, test_array));
  printf("%s\n", int_to_string(-2, test_array));
  printf("%s\n", int_to_string(-1, test_array));

  printf("%s\n", float_to_string(0.0f, 1, test_array));
  printf("%s\n", float_to_string(5.0f, 3, test_array));
  printf("%s\n", float_to_string(1.252f, 3, test_array));
  printf("%s\n", float_to_string(1.928401f, 3, test_array));
  printf("%s\n", float_to_string(1.928401f, 2, test_array));

  printf("%s\n", float_to_string(-5.0f, 3, test_array));
/*
  for(float p = 4.0f/3.0f; p <= 100.0f; p += 1.0f)
  {
    printf("%s\n", float_to_string(p, 3, test_array));
  }
*/

// End random test code

//
// Main body end
//

  // Reset video mode for dcload
  STARTUP_Set_Video(FB_RGB0555, USE_640x480);

  return 0; // startup.S doesn't care what this is
}
