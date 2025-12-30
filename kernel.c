// kernel.c - Our first C kernel
void main()
{
  // 1. Create a pointer to the video memory address.
  // 0xb8000 is the starting address for video memory in VGA text mode.
  char *video_memory = (char *)0xb8000;

  // 2. Write the character 'X' to the first position of the screen.
  // By dereferencing the pointer, we write directly to the hardware memory.
  *video_memory = 'X';
}