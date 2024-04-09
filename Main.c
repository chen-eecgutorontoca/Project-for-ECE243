#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


#define Maxsize 8
#define oneblockpixel 26

#define Timer 0xFF202000
#define HEX 0xFF200020
#define PushButton 0xFF200050
#define PS2 0xFF200108

#include "music.h"

#include "diffculty_page.h"
#include "bomb_png.h"
#include "flag_png.h"
#include "initialization.h"
#include "number_four.h"
#include "number_three.h"
#include "number_two.h"
#include "number_one.h"
#include "number_zero.h"
#include "win.h"
#include "right_img.h"
#include "left_img.h"

#include "nios2_ctrl_reg_macros.h"

extern const uint16_t diffculty[240][320];
extern const uint16_t initial_pic[240][320];
extern const uint16_t left[213][160];
extern const uint16_t right[216][160];
extern const uint16_t flag[25][25];
extern const uint16_t bomb[25][25];
extern const uint16_t four[25][25];
extern const uint16_t three[25][22];
extern const uint16_t two[25][34];
extern const uint16_t one[25][25];
extern const uint16_t zero[25][31];
extern const uint16_t winning[240][160];
extern const uint16_t you[240][160];

struct audio_t {
  volatile unsigned int control;
  volatile unsigned char rarc;
  volatile unsigned char ralc;
  volatile unsigned char warc;
  volatile unsigned char walc;
  volatile unsigned int ldata;
  volatile unsigned int rdata;
};

int samples_n = 0;
struct audio_t* const audiop = ((struct audio_t*)0xff203040);


//global variables
volatile int cnt = 0;
volatile bool End = false;  
char* Ps2data;  // Uncover Flag
char* Key;      // Up Down Right Left
int pixel_buffer_start;

// Function prototypes
void update_hex_display(int value);
int encode_digit(int digit);
void interrupt_handler(void);
void timer_ISR(void);
void audio_ISR();
void enable_interrupt(void);

void update_hex_display(int value) {
  volatile int* hex_ptr = (int*)HEX;

  int digit_0 = value % 10;
  int digit_1 = (value / 10) % 10;
  int digit_2 = (value / 100) % 10;
  int digit_3 = (value / 1000) % 10;

  int encoded_digits[4];
  encoded_digits[0] = encode_digit(digit_0);
  encoded_digits[1] = encode_digit(digit_1);
  encoded_digits[2] = encode_digit(digit_2);
  encoded_digits[3] = encode_digit(digit_3);

  int hex_value = 0;
  hex_value |= encoded_digits[0];
  hex_value |= encoded_digits[1] << 8;
  hex_value |= encoded_digits[2] << 16;
  hex_value |= encoded_digits[3] << 24;

  *hex_ptr = hex_value;
}

int encode_digit(int digit) {
  switch (digit) {
    case 0:
      return 0x3F;
    case 1:
      return 0x06;
    case 2:
      return 0x5B;
    case 3:
      return 0x4F;
    case 4:
      return 0x66;
    case 5:
      return 0x6D;
    case 6:
      return 0x7D;
    case 7:
      return 0x07;
    case 8:
      return 0x7F;
    case 9:
      return 0x6F;
    default:
      return 0x00;
  }
}

void interrupt_handler(void) {  // modified interrupt handler for Timer Interval and Audio
  int j;
  NIOS2_READ_IPENDING(j);
  if (j & 0x1) {
    timer_ISR();
  }
  if (j & 0x40) {
    audio_ISR();
  }
  return;
}

void timer_ISR(void) {
  volatile int* interval_timer_ptr = (int*)0xFF202000;

  *(interval_timer_ptr) = 0;

  if (End) {
    update_hex_display(0);
  } else {
    update_hex_display(cnt);
    cnt++;
  }
}

void audio_ISR(void) {
  while (audiop->warc == 0);
  audiop->ldata = samples[samples_n];
  audiop->rdata = samples[samples_n];
  samples_n++;
  if (samples_n == 100000) {
    samples_n = 0;
  }
}

void enable_interrupt(void) {
  volatile int* interval_timer_ptr = (int*)0xFF202000;
  int counter = 100000000;
  audiop->control = 0x8;  // clear the output FIFOs
  audiop->control = 0x0;  // resume input conversion
  *(interval_timer_ptr + 2) = (counter & 0xFFFF);
  *(interval_timer_ptr + 3) = (counter >> 16) & 0xFFFF;
  *(interval_timer_ptr + 1) = 0x7;
  volatile int* audio_ptr = (int*)0xFF203040;
  *(audio_ptr) = 0x2;
  NIOS2_WRITE_IENABLE(0x41);
  NIOS2_WRITE_STATUS(0x1);
}

char* NextStep(char* Ps2data);
char SearchBomb(char board[][9], char BoardStatus[][9], int row, int col);
void SearchZero(char board[][9], char BoardStatus[][9], int row, int col);
bool CheckValidity(char board[][9], char BoardStatus[][9], int row, int col,
                   char* nextstep);
void WhetherStart();
bool GameEnd(char board[][9], char BoardStatus[][9], char ChoseData);
void GameOverAnime();
void global_enable(void);  
void draw_grid();
void animations(bool end);
void clear_screen();
void start_page();
void difficultypage();
void animations_win();
void animations_lose();
void unflag(int x, int y);

void scan_code_decode(unsigned char scan_code) {
  switch (scan_code) {
    case 0x1C:  // A
      Ps2data = "Left";
      break;
    case 0x1B:  // S
                //   moveDown();
      Ps2data = "Down";
      break;
    case 0x23:  // D
                // moveRight();
      Ps2data = "Right";
      break;
    case 0x1D:  // W
                // moveUp();
                // m++;
      Ps2data = "Up";
      break;
    case 0x29:  // Space bar
                //  markFlag();
      Ps2data = "Space";
      break;
    case 0x5A:  // Enter
                // performSweep();
      Ps2data = "Enter";
      break;
    case 0x42:
      Ps2data = "Space";
      break;
    case 0x16:
      Ps2data = "level1";
      break;
    case 0x1E:
      Ps2data = "level2";
      break;
    case 0x26:
      Ps2data = "level3";
      break;
    default:
      Ps2data = "";
  }
}

void printMove(int posX, int posY, char state) {
  switch (state) {
    case 'F':
      for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 24; j++) {
          plot_pixel(44 + posX * oneblockpixel + j,
                     posY * oneblockpixel + i + 4, flag[i][j]);
        }
      }
      break;
    case 'B':
      for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 24; j++) {
          plot_pixel(44 + posX * oneblockpixel + j,
                     posY * oneblockpixel + i + 4, bomb[i][j]);
        }
      }
      break;
    case 'E':
      for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 24; j++) {
          plot_pixel(44 + posX * oneblockpixel + j,
                     posY * oneblockpixel + i + 4, zero[i][j]);
        }
      }
      break;
    case '2':
      for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 24; j++) {
          plot_pixel(44 + posX * oneblockpixel + j,
                     posY * oneblockpixel + i + 4, two[i][j]);
        }
      }
      break;
    case '3':
      for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 22; j++) {
          plot_pixel(44 + posX * oneblockpixel + j,
                     posY * oneblockpixel + i + 4, three[i][j]);
        }
      }
      break;
    case '4':
      for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 24; j++) {
          plot_pixel(44 + posX * oneblockpixel + j,
                     posY * oneblockpixel + i + 4, four[i][j]);
        }
      }
      break;
    case '1':
      for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 24; j++) {
          plot_pixel(44 + posX * oneblockpixel + j,
                     posY * oneblockpixel + i + 4, one[i][j]);
        }
      }
      break;
  }
}

void ps2_ISR() {
  unsigned char byte1 = 0;
  unsigned char byte2 = 0;
  unsigned char byte3 = 0;

  volatile int* PS2_ptr = (int*)0xFF200100;  // PS/2 port address

  int PS2_data, RVALID;
  while (1) {
    // 需要信号控制，不然死循环
    PS2_data = *(PS2_ptr);  // read the Data register in the PS/2 port

    RVALID = (PS2_data & 0x8000);  // extract the RVALID field

    if (RVALID != 0) {
      /* always save the last three bytes received */
      byte1 = byte2;
      byte2 = byte3;
      byte3 = PS2_data & 0xFF;
    }
    if ((byte2 == 0xAA) && (byte3 == 0x00)) {
      // mouse inserted; initialize sending of data
      *(PS2_ptr) = 0xF4;
    }
    if (byte2 == 0xF0) {
      break;
    }
  }
  // Display last byte on Red LEDs
  //*RLEDs = byte3;

  scan_code_decode(byte3);
}

int main() {
  bool End = false;  // End or not

  while (1) {
    enable_interrupt();
    volatile int* pixel_ctrl_ptr = (int*)0xFF203020;
    /* Read location of the pixel buffer from the pixel buffer controller */
    pixel_buffer_start = *pixel_ctrl_ptr;
    int x, y;
    x = 43;
    y = 3;

    clear_screen();
    start_page();

    char ChoseData = ' ';
    char* nextstep = "";
    int posX = 0;
    int posY = 0;
    End = false;  // End or not

    WhetherStart();
    clear_screen();  // send start request
    Ps2data = "";
    char OriginalBoard[9][9] = {{'B', '0', '0', '0', 'B', '0', '0', '0', 'B'},
                                {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                {'0', '0', '0', '0', '0', '0', '0', '0', '0'}};
    char OriginalBoard1[9][9] = {{'B', '0', '0', '0', 'B', '0', '0', '0', 'B'},
                                 {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                 {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                 {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                 {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                 {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                 {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                 {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
                                 {'0', '0', '0', '0', '0', '0', '0', '0', '0'}};
    char BoardStatus[9][9] = {
        {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
        {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
        {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
        {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
        {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
        {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
        {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
        {'0', '0', '0', '0', '0', '0', '0', '0', '0'},
        {'0', '0', '0', '0', '0', '0', '0', '0',
         '0'}};  // Have Status Flag F, Bomb B, Empty around
                 // E, 1,2,3,4 Bombs and 0 is cover status
    char OriginalBoard2[9][9] = {{'B', '0', '0', '0', '0', '0', '0', 'B', '0'},
                                 {'0', '0', 'B', '0', '0', '0', 'B', '0', '0'},
                                 {'0', '0', '0', '0', 'B', '0', '0', '0', '0'},
                                 {'0', '0', 'B', '0', '0', '0', '0', '0', '0'},
                                 {'0', '0', '0', 'B', '0', 'B', '0', 'B', '0'},
                                 {'0', '0', 'B', '0', '0', '0', '0', '0', '0'},
                                 {'0', 'B', '0', '0', '0', '0', '0', 'B', '0'},
                                 {'0', '0', 'B', '0', '0', '0', '0', '0', '0'},
                                 {'0', '0', '0', '0', 'B', '0', 'B', '0', '0'}};
    char OriginalBoard3[9][9] = {{'B', '0', '0', '0', 'B', '0', 'B', '0', 'B'},
                                 {'0', 'B', '0', '0', '0', '0', '0', 'B', '0'},
                                 {'0', '0', '0', 'B', '0', 'B', 'B', '0', '0'},
                                 {'0', 'B', '0', '0', '0', '0', '0', '0', '0'},
                                 {'0', '0', 'B', '0', '0', '0', '0', '0', 'B'},
                                 {'0', 'B', '0', '0', 'B', '0', 'B', '0', '0'},
                                 {'0', 'B', '0', '0', '0', '0', '0', 'B', '0'},
                                 {'0', '0', 'B', 'B', 'B', '0', '0', '0', '0'},
                                 {'B', 'B', '0', '0', '0', 'B', '0', '0', 'B'}};
    bool chooseonegrid = false;
    difficultypage();

    while (!chooseonegrid) {
      ps2_ISR();
      if (Ps2data == "level1") {
        for (int i = 0; i <= 8; i++) {
          for (int j = 0; j <= 8; j++) {
            OriginalBoard[i][j] = OriginalBoard1[i][j];
          }
        }

        chooseonegrid = true;
      }
      if (Ps2data == "level2") {
        for (int i = 0; i <= 8; i++) {
          for (int j = 0; j <= 8; j++) {
            OriginalBoard[i][j] = OriginalBoard2[i][j];
          }
        }
        chooseonegrid = true;
      }
      if (Ps2data == "level3") {
        for (int i = 0; i <= 8; i++) {
          for (int j = 0; j <= 8; j++) {
            OriginalBoard[i][j] = OriginalBoard3[i][j];
          }
        }
        chooseonegrid = true;
      }
    }
    clear_screen();
    draw_grid();
    choose_coordinate(posX, posY);
    while (!End) {
      ps2_ISR();
      nextstep = NextStep(Ps2data);  
      if (CheckValidity(OriginalBoard, BoardStatus, posX, posY, nextstep)) {
        eliminate_red_box(posX, posY);
        if (strcmp(nextstep, "Up") == 0) {
          posY--;
        }
        if (strcmp(nextstep, "Down") == 0) {
          posY++;
        }
        if (strcmp(nextstep, "Left") == 0) {
          posX--;
        }
        if (strcmp(nextstep, "Right") == 0) {
          posX++;
        }

        if (strcmp(nextstep, "Uncover") == 0) {
          ChoseData = SearchBomb(OriginalBoard, BoardStatus, posX, posY);
          BoardStatus[posX][posY] = ChoseData;
          if (ChoseData == 'B') {
            printMove(posX, posY, ChoseData);
          }
          if (ChoseData != 'B') {
            if (ChoseData == 'E') {
              SearchZero(OriginalBoard, BoardStatus, posX, posY);
            } else {
              BoardStatus[posX][posY] = ChoseData;
              printMove(posX, posY, ChoseData);
            }
          }
        }
        if (strcmp(nextstep, "Flag") == 0) {
          if (BoardStatus[posX][posY] == '0') {
            BoardStatus[posX][posY] = 'F';
            printMove(posX, posY, 'F');
          } else {
            BoardStatus[posX][posY] = '0';
            unflag(posX, posY);
          }
        }
        choose_coordinate(posX, posY);
      }
      End = GameEnd(OriginalBoard, BoardStatus, ChoseData);
      // printf("Hello");
      Ps2data = "";
    }
    audiop->control = 0x8;  // clear the output FIFOs
    audiop->control = 0x0;  // resume input conversion
    // audio_playback_mono(samples, samples_n);
    bool restart = false;
    if (ChoseData != 'B') {
      animations_win();
    } else {
      while (!restart) {
        ps2_ISR();
        if (Ps2data == "Space") {
          restart = true;
        }
      }
      Ps2data = "";
      animations_lose();
    }
    restart = false;
    while (!restart) {
      ps2_ISR();
      if (Ps2data == "Space") {
        restart = true;
      }
    }
    Ps2data = "";
  }
  return 0;
}

void choose_coordinate(int posX,
                       int posY) {  // x, y needs to be the new coordinate
  for (int i = 0; i <= 24; i++) {
    for (int j = 0; j < 1; j++) {
      plot_pixel(44 + posX * oneblockpixel + i, posY * oneblockpixel + j + 4,
                 0xF800);
    }
  }

  for (int i = 0; i <= 24; i++) {
    for (int j = 0; j < 1; j++) {
      plot_pixel(44 + posX * oneblockpixel + i,
                 posY * oneblockpixel + 24 + j + 4, 0xF800);
    }
  }

  for (int i = 0; i <= 24; i++) {
    for (int j = 0; j < 1; j++) {
      plot_pixel(44 + posX * oneblockpixel + j, posY * oneblockpixel + 4 + i,
                 0xF800);
    }
  }

  for (int i = 0; i <= 24; i++) {
    for (int j = 0; j < 1; j++) {
      plot_pixel(44 + posX * oneblockpixel + 24 + j,
                 posY * oneblockpixel + 4 + i, 0xF800);
    }
  }
}

void eliminate_red_box(int X, int Y) {  // x, y are the old coordinates
  for (int i = 0; i <= 24; i++) {
    for (int j = 0; j < 1; j++) {
      plot_pixel(44 + X * oneblockpixel + i, Y * oneblockpixel + j + 4, 0xffff);
    }
  }

  for (int i = 0; i <= 24; i++) {
    for (int j = 0; j < 1; j++) {
      plot_pixel(44 + X * oneblockpixel + i, Y * oneblockpixel + 24 + j + 4,
                 0xffff);
    }
  }

  for (int i = 0; i <= 24; i++) {
    for (int j = 0; j < 1; j++) {
      plot_pixel(44 + X * oneblockpixel + j, Y * oneblockpixel + 4 + i, 0xffff);
    }
  }

  for (int i = 0; i <= 24; i++) {
    for (int j = 0; j < 1; j++) {
      plot_pixel(44 + X * oneblockpixel + 24 + j, Y * oneblockpixel + 4 + i,
                 0xffff);
    }
  }
}

// to use this function, set a if condition to verify if flag has been in this
// (x,y) position, if yes, call the unflag function
void unflag(int x, int y) {
  for (int i = 0; i < 25; i++) {
    for (int j = 0; j <= 24; j++) {
      plot_pixel(44 + x * oneblockpixel + j, y * oneblockpixel + i + 4, 0xffff);
    }
  }
}

void clear_screen() {
  for (int i = 0; i < 320; i++) {
    for (int j = 0; j < 240; j++) {
      plot_pixel(i, j, 0xffff);
    }
  }
}

void plot_pixel(int x, int y, short int line_color) {
  volatile short int* one_pixel_address;

  one_pixel_address = pixel_buffer_start + (y << 10) + (x << 1);

  *one_pixel_address = line_color;
}

void start_page() {
  for (int i = 0; i < 240; i++) {
    for (int j = 0; j < 320; j++) {
      plot_pixel(j, i, initial_pic[i][j]);
    }
  }
}

void difficultypage() {
  for (int i = 0; i < 240; i++) {
    for (int j = 0; j < 320; j++) {
      plot_pixel(j, i, diffculty[i][j]);
    }
  }
}

void WhetherStart() {
  // Ps2data = Keyboard input
  bool start = false;
  while (!start) {
    printf("wait");
    ps2_ISR();
    if (strcmp(Ps2data, "Space") == 0) {
      start = true;
    }
  }
  return;
}
char* NextStep(char* Ps2data) {
  char* next = "";
  if (strcmp(Ps2data, "Space") == 0) {
    next = "Uncover";
  }
  if (strcmp(Ps2data, "Enter") == 0) {
    next = "Flag";
  }
  if (strcmp(Ps2data, "Up") == 0 || strcmp(Ps2data, "Down") == 0 ||
      strcmp(Ps2data, "Left") == 0 || strcmp(Ps2data, "Right") == 0) {
    next = Ps2data;
  }
  return next;
}
bool CheckValidity(char board[][9], char BoardStatus[][9], int row, int col,
                   char* nextstep) {
  bool move = false;
  if (strcmp(nextstep, "Up") == 0) {
    col--;
    move = true;
  }
  if (strcmp(nextstep, "Down") == 0) {
    col++;
    move = true;
  }
  if (strcmp(nextstep, "Left") == 0) {
    row--;
    move = true;
  }
  if (strcmp(nextstep, "Right") == 0) {
    row++;
    move = true;
  }
  if (row < 0 || row > 8 || col < 0 || col > 8) {
    // false judgement
    return false;
  } else if (move) {
    return true;
  }
  if (strcmp(nextstep, "Uncover") == 0) {
    if (BoardStatus[row][col] == '0') {
      return true;
    } else {
      return false;
    }
  }
  if (strcmp(nextstep, "Flag") == 0) {
    if (BoardStatus[row][col] == '0' || BoardStatus[row][col] == 'F') {
      return true;
    } else {
      return false;
    }
  }
  return false;
}

char SearchBomb(char board[][9], char BoardStatus[][9], int row, int col) {
  char Num = '0';
  int Row = 0;
  int Col = 0;
  if (board[row][col] == 'B') {
    return 'B';
  }
  for (int i = -1; i <= 1; i++) {
    for (int j = -1; j <= 1; j++) {
      Row = row + i;
      Col = col + j;
      if (Row < 0 || Row > 8 || Col < 0 || Col > 8 ||
          (Row == row && Col == col)) {
        continue;
      } else if (board[Row][Col] == 'B') {
        Num++;
      }
    }
  }
  printf("%c", Num);
  if (Num == '0') {
    return 'E';
  }
  return Num;
}

void SearchZero(char board[][9], char BoardStatus[][9], int row, int col) {
  if (row < 0 || row > 8 || col < 0 || col > 8) {
    return;
  }
  int Row = 0;
  int Col = 0;
  for (int i = -1; i <= 1; i++) {
    for (int j = -1; j <= 1; j++) {
      Row = row + i;
      Col = col + j;
      if (Row < 0 || Row > 8 || Col < 0 || Col > 8) {
        continue;
      } else if (board[Row][Col] == 'B') {
        printf("have bomb");
        return;
      }
    }
  }
  BoardStatus[row][col] = 'E';  // Empty bomb in the Vicinity
  printMove(row, col, 'E');
  for (int m = -1; m <= 1; m++) {
    for (int n = -1; n <= 1; n++) {
      if ((m != 0 || n != 0) &&
          ((row + m) >= 0 && (row + m) <= 8 && (col + n) >= 0 &&
           (col + n) <= 8) &&
          BoardStatus[row + m][col + n] == '0') {
        printf("recursion");
        SearchZero(board, BoardStatus, row + m, col + n);
      }
    }
  }

  return;
}
bool GameEnd(char board[][9], char BoardStatus[][9], char ChoseData) {
  int BombLEFT = 0;
  int BombNum = 0;
  if (ChoseData == 'B') {
    printf("you lose");
    return true;
  }
  for (int i = 0; i <= 8; i++) {
    for (int j = 0; j <= 8; j++) {
      if (BoardStatus[i][j] == '0' || BoardStatus[i][j] == 'F') {
        BombLEFT++;
      }
      if (board[i][j] == 'B') {
        BombNum++;
      }
    }
  }
  if (BombNum == BombLEFT) {
    return true;
    printf("you win");
  } else {
    return false;
  }
}

void vsyncWait() {
  volatile int* pixel_ctrl_ptr = (int*)0xFF203020;
  *pixel_ctrl_ptr = 1;
  int status = *(pixel_ctrl_ptr + 3) & 1;
  while (status != 0) {
    status = *(pixel_ctrl_ptr + 3) & 1;
  }
}

void draw_grid() {
  for (int i = 3; i < 238; i++) {
    plot_pixel(43, i, 0x000);
  }

  for (int i = 3; i < 238; i++) {
    plot_pixel(69, i, 0x000);
  }

  for (int i = 3; i < 238; i++) {
    plot_pixel(95, i, 0x000);
  }

  for (int i = 3; i < 238; i++) {
    plot_pixel(121, i, 0x000);
  }

  for (int i = 3; i < 238; i++) {
    plot_pixel(147, i, 0x000);
  }

  for (int i = 3; i < 238; i++) {
    plot_pixel(173, i, 0x000);
  }

  for (int i = 3; i < 238; i++) {
    plot_pixel(199, i, 0x000);
  }

  for (int i = 3; i < 238; i++) {
    plot_pixel(225, i, 0x000);
  }

  for (int i = 3; i < 238; i++) {
    plot_pixel(251, i, 0x000);
  }

  for (int i = 3; i < 238; i++) {
    plot_pixel(277, i, 0x000);
  }

  for (int j = 40; j < 281; j++) {
    plot_pixel(j, 29, 0x000);
  }

  for (int j = 40; j < 281; j++) {
    plot_pixel(j, 55, 0x000);
  }

  for (int j = 40; j < 281; j++) {
    plot_pixel(j, 81, 0x000);
  }

  for (int j = 40; j < 281; j++) {
    plot_pixel(j, 107, 0x000);
  }

  for (int j = 40; j < 281; j++) {
    plot_pixel(j, 133, 0x000);
  }

  for (int j = 40; j < 281; j++) {
    plot_pixel(j, 159, 0x000);
  }

  for (int j = 40; j < 281; j++) {
    plot_pixel(j, 185, 0x000);
  }

  for (int j = 40; j < 281; j++) {
    plot_pixel(j, 211, 0x000);
  }
}



void audio_playback_mono(int* samples, int n) {
  int i;
  audiop->control = 0x8;  // clear the output FIFOs
  audiop->control = 0x0;  // resume input conversion
  for (i = 0; i < n; i++) {
    // wait till there is space in the output FIFO
    while (audiop->warc == 0)
      ;
    audiop->ldata = samples[i];
    audiop->rdata = samples[i];
  }  
}

void animations_lose() {  // animation for losing
  extern const uint16_t left[213][160];
  extern const uint16_t right[216][160];

  short int Buffer1[240][512];
  short int Buffer2[240][512];

  volatile int* pixel_ctrl_ptr = (int*)0xFF203020;

  *(pixel_ctrl_ptr + 1) = (int)&Buffer1;

  vsyncWait();

  pixel_buffer_start = *pixel_ctrl_ptr;
  clear_screen();

  *(pixel_ctrl_ptr + 1) = (int)&Buffer2;
  pixel_buffer_start = *(pixel_ctrl_ptr + 1);
  clear_screen();

  int x_left = -160;
  int x_right = 830;
  int count = 0;
  int speed = 6;
  int y_offset = 11;

  while (1) {
    for (int y = 0; y < 213; y++) {
      for (int x = 0; x < 160; x++) {
        plot_pixel(x + x_left, y + y_offset, left[y][x]);
      }
    }

    for (int y = 0; y < 213; y++) {
      for (int x = 0; x < 160; x++) {
        plot_pixel(x + x_right, y + y_offset - 1, right[y][x]);
      }
    }

    x_left += speed;
    x_right -= speed;
    count++;
    // printf("\n%d",count);

    vsyncWait();

    pixel_buffer_start = *(pixel_ctrl_ptr + 1);

    if (count > 27) {
      break;
    }
  }
}

void animations_win() {  // animation for winning
  extern const uint16_t winning[240][160];
  extern const uint16_t you[240][160];

  short int Buffer1[240][512];
  short int Buffer2[240][512];

  volatile int* pixel_ctrl_ptr = (int*)0xFF203020;

  *(pixel_ctrl_ptr + 1) = (int)&Buffer1;

  vsyncWait();

  pixel_buffer_start = *pixel_ctrl_ptr;
  clear_screen();

  *(pixel_ctrl_ptr + 1) = (int)&Buffer2;
  pixel_buffer_start = *(pixel_ctrl_ptr + 1);
  clear_screen();

  int x_left = -160;
  int x_right = 830;
  int count = 0;
  int speed = 6;
  int y_offset = 11;

  while (1) {
    for (int y = 0; y < 213; y++) {
      for (int x = 0; x < 160; x++) {
        plot_pixel(x + x_left, y + y_offset, you[y][x]);
      }
    }

    for (int y = 0; y < 213; y++) {
      for (int x = 0; x < 160; x++) {
        plot_pixel(x + x_right, y + y_offset - 1, winning[y][x]);
      }
    }

    x_left += speed;
    x_right -= speed;
    count++;
    // printf("\n%d",count);

    vsyncWait();

    pixel_buffer_start = *(pixel_ctrl_ptr + 1);

    if (count > 27) {
      break;
    }
  }
}