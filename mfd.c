#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

/// HID Gamepad Protocol Report.
typedef struct TU_ATTR_PACKED
{
  uint16_t buttons; ///< Buttons mask for currently pressed buttons
} hid_gamepad_report_t;

//Row, Columns to GPIO
#define C0 16
#define C1 13
#define C2 18
#define C3 17
#define C4 12
#define C5 11
#define R0 15
#define R1 14
#define R2 9
#define R3 10

#define COLUMN_SIZE 6
#define ROW_SIZE 4

#define OFF 0
#define ON 1
#define CONFIRMED 2

uint8_t columns[] = {C0, C1, C2, C3, C4, C5};
uint8_t rows[] = {R0, R1, R2, R3};

// State Machine
typedef enum {UPDATEBUTTONS, DEBOUNCEDELAY, DEBOUNCE} states_t;
uint8_t current_column = 1;

//Buttons
#define DEBOUNCE_DELAY 100
#define POLL_DELAY 800
#define BUTTON_COUNT 24

uint8_t buttons[BUTTON_COUNT];
uint64_t buttonDebounce[BUTTON_COUNT];
uint32_t button_mask = 0;

static const unsigned button_matrix[COLUMN_SIZE][ROW_SIZE] = {
    {0, 6, 12, 18},
    {1, 7, 13, 23},
    {2, 8, 14, 21},
    {3, 9, 15, 22},
    {4, 10, 16, 20},
    {5, 11, 17, 19}
};

void init_gpio()
{
    for (int gpio = 9; gpio <= 18; gpio++){
        gpio_init(gpio);
        gpio_set_dir(gpio, GPIO_IN);     
    }

    gpio_set_dir(columns[0], GPIO_OUT);
    gpio_put(columns[0], 1);
}

void button_pressed(uint8_t num){
    uint32_t btn = (1 << num);
    button_mask = button_mask | btn;
}

void button_released(uint8_t num) {
    uint32_t btn = !(1 << num);
    button_mask = button_mask & btn;  
}

void update_button(uint8_t num, uint8_t current) 
{
    uint8_t last = buttons[num];

    if(last == ON && current == OFF)
    {
        buttons[num] = OFF;
    }

    if(last == OFF && current == ON){
        buttons[num] = ON;
    }

    if(last == CONFIRMED && current == OFF)
    {
        button_released(num);
        buttons[num] = OFF;
    }
    
}

void buttons_task(){
    static states_t state = UPDATEBUTTONS;
    static uint64_t delay_start = 0;
    static uint64_t poll_delay = 0;

    switch(state){
        case UPDATEBUTTONS:
            if (poll_delay == 0)
            {
                poll_delay = time_us_64();
            }
            else
            {
                if(time_us_64() > poll_delay+POLL_DELAY){
                    poll_delay = 0;
                  
                    for(int i = 0; i < ROW_SIZE; i++){
                        bool btn_state = gpio_get(rows[i]);
                        update_button(button_matrix[current_column-1][i],  btn_state ? ON : OFF);
                    }
                    state = DEBOUNCEDELAY;
                }
            }                           
        break;

        case DEBOUNCEDELAY:
            if (delay_start == 0)
            {
                delay_start = time_us_64();
            }
            else
            {
                if( time_us_64() > delay_start+DEBOUNCE_DELAY){
                    delay_start = 0;
                    state = DEBOUNCE; 
                }
            }        
        break;
        
        case DEBOUNCE:
            for(int i = 0; i < ROW_SIZE; i++){
                uint8_t current_num = button_matrix[current_column-1][i];
                uint8_t current_state = gpio_get(rows[i]) == 0 ? OFF : ON;
                
                if(current_state == OFF)
                {
                    buttons[current_num] = OFF;
                }
                else 
                {
                    if(buttons[current_num] == ON)
                    {
                        buttons[current_num] = CONFIRMED;
                        button_pressed(current_num);
                    }                  
                }
            }
            
            //Set current gpio pin to floating
            gpio_put(columns[current_column-1], 0);
            gpio_set_dir(columns[current_column-1], GPIO_IN);  

            //Goto next column    
            current_column++;
            if(current_column > COLUMN_SIZE)
            {
                current_column = 1;
            }

            //Set current gpio pin to output
            gpio_set_dir(columns[current_column-1], GPIO_OUT);      
            gpio_put(columns[current_column-1], 1);

            state = UPDATEBUTTONS; 
        break;
    }
}

//Send updates to computer
void hid_task(void)
{
    //Send first or second part of button mask
    static bool toggle = false;

    // Poll every 10ms
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;

    if (board_millis() - start_ms < interval_ms)
    return;
    start_ms += interval_ms;

    // Remote wakeup
    if (tud_suspended()) {
        tud_remote_wakeup();
    }

    if (tud_hid_ready())
    {          
        if(!toggle)
        {
            hid_gamepad_report_t report =
            {
                .buttons = button_mask
            };
            tud_hid_report(1, &report, sizeof(report));
        } 
        else 
        {
            hid_gamepad_report_t report =
            {
                .buttons = button_mask>>16
            };
            tud_hid_report(2, &report, sizeof(report));
        }

        //Switch
        toggle = !toggle;
    }
}

int main()
{
    board_init();  
    tusb_init();
    init_gpio();

    while(1){     
        tud_task();
        hid_task();
        buttons_task();
    }
}

//UNUSED
// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
  // TODO set LED based on CAPLOCK, NUMLOCK etc...
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)bufsize;
}