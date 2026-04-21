/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "stdio.h"
#include "string.h"
#include "math.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
// Bản đồ phím Keypad
uint8_t keypad_map[4][4] = {
    {'1','2','3','+'},
    {'4','5','6','-'},
    {'7','8','9','x'},
    {'*','0','#','/'}
};

char msg[50];        // Bộ đệm gửi UART
char display_msg[20] = "Ready...";
char time_now[15], date_now[15]; // Thêm 2 dòng này để lưu chuỗi giờ/ngày

uint8_t is_setting_time = 0; // 0: Chế độ xem giờ, 1: Chế độ chỉnh giờ
uint8_t input_count = 0;     // Đếm xem đã nhập bao nhiêu số
char input_buffer[5];        // Lưu 4 số nhập vào (HHMM)
uint8_t mode= 0;				//0:đong hồ và máy tính; 1: chỉnh giờ
uint32_t pressTime = 0;			// kiểm tra thời gian đè phím A
uint8_t isPress = 0;
float num1 = 0, num2 = 0, result = 0;
char op = 0;
uint8_t digit_count=0;
char str_num1[10] = "";
char str_num2[10] = "";
uint8_t state = 0; 			// trạng thái nhập: 0: nhập num1; 2: nhập num2; 3: sau khi nhấn "="
uint32_t lastKeyTime = 0;
char error_msg[16];
float temp = 0;
uint8_t has_num1 = 0;
uint8_t has_result = 0;
// Biến dùng cho tính năng Nhận lệnh UART
uint8_t rx_byte;         // Chứa 1 ký tự vừa nhận được
char rx_buf[32];         // Cuốn sổ ghi chép lại nguyên 1 dòng lệnh (VD: "SET 14:30")
uint8_t rx_index = 0;    // Đếm xem đã ghi được bao nhiêu chữ
uint8_t cmd_ready = 0;   // Báo hiệu: 0 = chưa có lệnh, 1 = đã có lệnh hoàn chỉnh
float nums[10];     // Lưu tối đa 10 số trong 1 phép tính
char ops[10];       // Lưu tối đa 10 phép toán
int step_idx = 0;   // Đếm xem đang ở bước thứ mấy
uint8_t is_typing_number = 0; // Cờ kiểm tra đang gõ dở một số
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void Get_Time(char* time_str, char* date_str);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// --- 1. BIẾN TOÀN CỤC CHO MÀN HÌNH ---
char calc_display_str[64] = "";

// --- 2. CÁC HÀM CƠ BẢN ---
void UART_Print(char* str) {
    HAL_UART_Transmit(&huart2, (uint8_t*)str, strlen(str), 100);
}

char Keypad_Scan(void) {
    for(int i=0; i<4; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, (GPIO_PIN_12 << i), GPIO_PIN_RESET);
        if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == GPIO_PIN_RESET) return keypad_map[i][0];
        if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_RESET) return keypad_map[i][1];
        if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_RESET) return keypad_map[i][2];
        if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11) == GPIO_PIN_RESET) return keypad_map[i][3];
    }
    return 0;
}

uint8_t Button_Handle(char key){
    if(key == '+') {
        if(isPress==0) { pressTime = HAL_GetTick(); isPress = 1; }
        else if(isPress==1 && (HAL_GetTick() - pressTime > 2000)){
            mode = (mode == 0) ? 1 : 0; isPress = 2; return 2;
        }
        return 0;
    } else {
        if (isPress == 1) { isPress = 0; return 1; }
        isPress = 0; return 0;
    }
}

void format_num(char* buf, float val) {
    int int_part = (int)val;
    if (val == int_part) sprintf(buf, "%d", int_part);
    else {
        int dec_part = (int)(fabs(val - int_part) * 100);
        if (val < 0 && int_part == 0) sprintf(buf, "-0.%02d", dec_part);
        else sprintf(buf, "%d.%02d", int_part, dec_part);
    }
}

// --- 3. LOGIC MÁY TÍNH ---
void Build_Display_String() {
    char temp_str[15];
    calc_display_str[0] = '\0';

    for (int i = 0; i <= step_idx; i++) {
        if (i < step_idx || is_typing_number) {
            format_num(temp_str, nums[i]);
            strcat(calc_display_str, temp_str);
        }
        if (i < step_idx) {
            int len = strlen(calc_display_str);
            calc_display_str[len] = ' ';
            calc_display_str[len+1] = ops[i];
            calc_display_str[len+2] = ' ';
            calc_display_str[len+3] = '\0';
        }
    }
}

float Smart_Calculator() {
    float temp_n[10];
    char temp_o[10];
    int t_idx = 0;

    temp_n[0] = nums[0];
    for (int i = 0; i < step_idx; i++) {
        if (ops[i] == 'x') {
            temp_n[t_idx] = temp_n[t_idx] * nums[i+1];
        }
        else if (ops[i] == '/') {
            if (nums[i+1] == 0) return INFINITY;
            temp_n[t_idx] = temp_n[t_idx] / nums[i+1];
        }
        else {
            temp_o[t_idx] = ops[i];
            t_idx++;
            temp_n[t_idx] = nums[i+1];
        }
    }

    float final_result = temp_n[0];
    for (int i = 0; i < t_idx; i++) {
        if (temp_o[i] == '+') final_result += temp_n[i+1];
        if (temp_o[i] == '-') final_result -= temp_n[i+1];
    }
    return final_result;
}

void Calculator_HandleKey(char key) {
	// --- 1. CHỐT CHẶN LỖI (KHIẾN BÀN PHÍM ĐÓNG BĂNG NẾU CÓ LỖI) ---
	    if (error_msg[0] != '\0') {
	        if (key == '*') {
	            for(int i=0; i<10; i++) { nums[i] = 0; ops[i] = 0; }
	            step_idx = 0;
	            is_typing_number = 0;
	            has_result = 0;
	            error_msg[0] = '\0';
	        }
	        return;
	    }

	    // --- 2. PHÍM CLEAR (*) ---
	    if (key == '*') {
	        for(int i=0; i<10; i++) { nums[i] = 0; ops[i] = 0; }
	        step_idx = 0;
	        is_typing_number = 0;
	        has_result = 0;
	        return;
	    }

	    // --- 3. PHÍM SỐ (0-9) - FIX LỖI TRÀN 7 SỐ TẠI ĐÂY ---
	    if (key >= '0' && key <= '9') {
	        if (has_result == 1) {
	            nums[0] = 0;
	            has_result = 0;
	        }

	        // Tính thử xem nếu thêm số này vào có bị tràn không
	        float temp_val = nums[step_idx] * 10 + (key - '0');
	        if (temp_val > 9999999 || temp_val < -9999999) {
	            sprintf(error_msg, "Too large!");
	            UART_Print("[ERR] Input Too Large!\r\n");
	            return;
	        }

	        nums[step_idx] = temp_val;
	        is_typing_number = 1;
	    }

	    // --- 4. PHÍM PHÉP TÍNH (+ - x /) ---
	    else if (key == '+' || key == '-' || key == 'x' || key == '/') {
	        has_result = 0;

	        // FIX LỖI +2 và -3 TẠI ĐÂY: Bấm phép tính khi chưa có số nào
	        if (!is_typing_number && step_idx == 0) {
	            if (key == 'x' || key == '/') {
	                sprintf(error_msg, "No num1!");
	                UART_Print("[ERR] No num1!\r\n");
	                return;
	            } else if (key == '+' || key == '-') {
	                // Cho phép nhập +2 hoặc -3 ngay từ đầu (Hiểu ngầm là: 0 + X hoặc 0 - X)
	                nums[0] = 0;
	                ops[0] = key;
	                step_idx = 1;
	                return;
	            }
	        }

	        if (is_typing_number) {
	            ops[step_idx] = key;
	            step_idx++;
	            is_typing_number = 0;
	        } else if (step_idx > 0) {
	            ops[step_idx - 1] = key;
	        }
	    }

	    // --- 5. PHÍM BẰNG (#) ---
	    else if (key == '#') {
	        if (!is_typing_number && step_idx > 0) {
	            sprintf(error_msg, "No num2!");
	            UART_Print("[ERR] No num2!\r\n");
	            return;
	        }

	        if (is_typing_number && step_idx > 0) {
	            Build_Display_String();
	            float result = Smart_Calculator();

	            if (isinf(result) || isnan(result)) {
	                sprintf(error_msg, "Div by 0!");
	                UART_Print("[ERR] Math Error: Div by 0!\r\n");
	                return;
	            }

	            // FIX LỖI KẾT QUẢ BỊ TRÀN
	            if (result > 9999999 || result < -9999999) {
	                sprintf(error_msg, "Overflow!");
	                UART_Print("[ERR] Result Overflow!\r\n");
	                return;
	            }

	            char uart_buf[128];
	            sprintf(uart_buf, "[MAY TINH] %s = %.2f\r\n", calc_display_str, result);
	            UART_Print(uart_buf);

	            for(int i=0; i<10; i++) { nums[i] = 0; ops[i] = 0; }
	            nums[0] = result;
	            step_idx = 0;
	            is_typing_number = 1;
	            has_result = 1;
	        }
	        else if (is_typing_number && step_idx == 0) {
	            has_result = 1;
	        }
	    }
}

// --- 4. LOGIC ĐỒNG HỒ ---
typedef struct {
    uint8_t seconds; uint8_t minutes; uint8_t hour; uint8_t dayofweek;
    uint8_t dayofmonth; uint8_t month; uint8_t year;
} TIME;

TIME time;

uint8_t bcdToDec(uint8_t val) { return ( (val/16*10) + (val%16) ); }
uint8_t decToBcd(uint8_t val) { return ( (val/10*16) + (val%10) ); }

void Set_Time(uint8_t sec, uint8_t min, uint8_t hour, uint8_t dow, uint8_t dom, uint8_t month, uint8_t year) {
    uint8_t set_time[7];
    set_time[0] = decToBcd(sec); set_time[1] = decToBcd(min); set_time[2] = decToBcd(hour);
    set_time[3] = decToBcd(dow); set_time[4] = decToBcd(dom); set_time[5] = decToBcd(month); set_time[6] = decToBcd(year);
    HAL_I2C_Mem_Write(&hi2c1, 0xD0, 0x00, 1, set_time, 7, 1000);
}

void Get_Time(char* time_str, char* date_str) {
    uint8_t get_time[7];
    if (HAL_I2C_Mem_Read(&hi2c1, 0xD0, 0x00, 1, get_time, 7, 1000) == HAL_OK) {
        uint8_t s = bcdToDec(get_time[0]); uint8_t m = bcdToDec(get_time[1]); uint8_t h = bcdToDec(get_time[2]);
        uint8_t day = bcdToDec(get_time[4]); uint8_t month = bcdToDec(get_time[5]); uint8_t year = bcdToDec(get_time[6]);
        time.dayofmonth = day; time.month = month; time.year = year;
        sprintf(time_str, "%02d:%02d:%02d", h, m, s);
        sprintf(date_str, "%02d/%02d/20%02d", day, month, year);
    } else {
        sprintf(time_str, "RTC Error"); sprintf(date_str, "00/00/0000");
    }
}

void Process_Time_Setting(char key) {
    if (key >= '0' && key <= '9' && input_count < 4) {
        input_buffer[input_count++] = key; input_buffer[input_count] = '\0';
    } else if (key == '*') {
        input_count = 0; input_buffer[0] = '\0';
    } else if (key == '#') {
        if (input_count == 4) {
            int h = (input_buffer[0] - '0') * 10 + (input_buffer[1] - '0');
            int m = (input_buffer[2] - '0') * 10 + (input_buffer[3] - '0');
            if (h < 24 && m < 60) {
                Set_Time(0, m, h, 5, time.dayofmonth, time.month, time.year);
                mode = 0; input_count = 0;
            }
        }
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
      ssd1306_Init();
      // Cài đặt: Giây, Phút, Giờ, Thứ, Ngày, Tháng, Năm (Gỡ dòng này sau khi đã nạp 1 lần)
      // Bắt đầu lắng nghe UART (Kích hoạt ngắt nhận)
            HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  char key = Keypad_Scan(); // Luôn quét phím mỗi vòng lặp
	  // -----> BẮT ĐẦU ĐOẠN XỬ LÝ LỆNH TỪ PC <-----
	        if (cmd_ready == 1) {
	            // Kiểm tra xem lệnh gửi xuống có đúng chữ "SET " không (VD: "SET 14:30")
	            if (strncmp((char*)rx_buf, "SET ", 4) == 0 && strlen((char*)rx_buf) >= 9) {
	                // Cắt lấy giờ (Vị trí số 4 và 5) và phút (Vị trí số 7 và 8)
	                int h = (rx_buf[4] - '0') * 10 + (rx_buf[5] - '0');
	                int m = (rx_buf[7] - '0') * 10 + (rx_buf[8] - '0');

	                if (h >= 0 && h < 24 && m >= 0 && m < 60) {
	                    // Cập nhật vào chip RTC
	                    Set_Time(0, m, h, 5, time.dayofmonth, time.month, time.year);
	                    UART_Print("[SYS] Time Updated from PC!\r\n");
	                } else {
	                    UART_Print("[ERR] Invalid Time format!\r\n");
	                }
	            } else {
	                UART_Print("[ERR] Unknown Command. Try 'SET HH:MM'\r\n");
	            }

	            // Xử lý xong thì xóa sổ, dọn cờ để đón lệnh tiếp theo
	            rx_index = 0;
	            cmd_ready = 0;
	        }
	        // -----> KẾT THÚC ĐOẠN XỬ LÝ LỆNH TỪ PC <-----
	  uint8_t btn = Button_Handle(key);
	  uint8_t showClock = 0;
	  if (mode == 0) {
		  char effective_key = key;
		  if ( effective_key != 0) {
		      lastKeyTime = HAL_GetTick();
		      // Nếu màn hình đang là đồng hồ, phím đầu tiên chỉ dùng để đánh thức, xóa phím đó đi
		      if (showClock == 1) {
		          showClock = 0;
		          effective_key = 0;
		      }
		  }
		  if ( HAL_GetTick() - lastKeyTime > 15000 ) showClock = 1;

		  if ( showClock == 0 )
		  {
			  if (key == '+') effective_key = 0; // đè A thì effective_key không nhận giá trị tránh nhảy dấu cộng trên màn hình
			  if (key == 0 && btn ==1) effective_key = '+'; // nhấn A nhưng nhả ra nhanh
			  if ( effective_key != 0) {
				  Calculator_HandleKey(effective_key);
				  HAL_Delay(200);
			  }
			  char buffer[32];
			  char short_time[6];
			  Get_Time(time_now, date_now);
			  ssd1306_Fill(Black);
			  ssd1306_SetCursor(0, 0);
			  ssd1306_WriteString("-CALC-", Font_7x10, White);
			  snprintf(short_time, sizeof(short_time), "%c%c:%c%c",
					  time_now[0], time_now[1],
					  time_now[3], time_now[4]);
			  ssd1306_SetCursor(85, 0);
			  ssd1306_WriteString(short_time, Font_7x10, White);
			  if (error_msg[0] != '\0') {
			          // --- HIỂN THỊ LỖI ---
			          ssd1306_SetCursor(10, 20);
			          ssd1306_WriteString(error_msg, Font_11x18, White);
			          ssd1306_SetCursor(0, 50);
			          ssd1306_WriteString("*:Clear", Font_6x8, White);
			          ssd1306_UpdateScreen();
			      }
			  else {
				  // Cập nhật chuỗi phép tính
				                    Build_Display_String();

				                    // In chuỗi phép tính (Ví dụ: 2 + 3 / 3)
				                    ssd1306_SetCursor(0, 14);
				                    ssd1306_WriteString(calc_display_str, Font_7x10, White);

				                    // In kết quả nếu có
				                    char result_str[12];
				                    format_num(result_str, nums[0]); // Vì kết quả cuối cùng đã gán ngược vào nums[0]

				                    char buffer[32];
				                    sprintf(buffer, "= %s", (has_result ? result_str : ""));
				                    ssd1306_SetCursor(0, 28);
				                    ssd1306_WriteString(buffer, Font_11x18, White);

				                    ssd1306_SetCursor(0, 50);
				                    ssd1306_WriteString("#:=  *:Clear", Font_6x8, White); // Đổi hướng dẫn một xíu
				                    ssd1306_UpdateScreen();
			  }
			  HAL_Delay(50);
		  }
		  if ( showClock == 1 ) {
			  Get_Time(time_now, date_now);
			  ssd1306_Fill(Black);
			  	            ssd1306_SetCursor(25, 0);
			  	            ssd1306_WriteString(date_now, Font_7x10, White);
			  	            ssd1306_SetCursor(18, 18);
			  	            ssd1306_WriteString(time_now, Font_11x18, White);
			  	            ssd1306_SetCursor(10, 48);
			  	            ssd1306_WriteString(display_msg, Font_7x10, White);
			  	            ssd1306_UpdateScreen();
		  }
	  }
	  else {
	            // --- CHẾ ĐỘ 2: MENU CHỈNH GIỜ ---
	        	HAL_Delay(200);
	            if (key != 0) {
	                Process_Time_Setting(key);
	                HAL_Delay(200);
	            }

	            ssd1306_Fill(Black);
	            ssd1306_SetCursor(10, 0);
	            ssd1306_WriteString("SET TIME (HHMM)", Font_7x10, White);

	            // Hiển thị định dạng HH:MM để người dùng dễ nhìn
	            char temp_show[10];
	            sprintf(temp_show, "%c%c:%c%c",
	                    (input_count > 0 ? input_buffer[0] : '_'),
	                    (input_count > 1 ? input_buffer[1] : '_'),
	                    (input_count > 2 ? input_buffer[2] : '_'),
	                    (input_count > 3 ? input_buffer[3] : '_'));

	            ssd1306_SetCursor(35, 22);
	            ssd1306_WriteString(temp_show, Font_11x18, White);

	            ssd1306_SetCursor(0, 50);
	            ssd1306_WriteString("#:Save  *:Clear", Font_7x10, White);
	            ssd1306_UpdateScreen();

	        HAL_Delay(50); // Giảm Delay xuống để phím bấm nhạy hơn
	  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Oled_CS_GPIO_Port, Oled_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, Oled_RES_Pin|Oled_DC_Pin|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin : Oled_CS_Pin */
  GPIO_InitStruct.Pin = Oled_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Oled_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : Oled_RES_Pin Oled_DC_Pin PB12 PB13
                           PB14 PB15 */
  GPIO_InitStruct.Pin = Oled_RES_Pin|Oled_DC_Pin|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA9 PA10 PA11 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// Hàm này tự động chạy mỗi khi nhận được 1 chữ từ máy tính
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        if (rx_byte == '\n' || rx_byte == '\r') {
            if (rx_index > 0) {
                rx_buf[rx_index] = '\0';
                cmd_ready = 1;
            }
        } else {
            if (rx_index < 30) {
                rx_buf[rx_index++] = rx_byte;
            }
        }
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}

// Hàm "cấp cứu": Tự động khởi động lại bộ lắng nghe nếu bị lỗi nghẹn dữ liệu
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        __HAL_UART_CLEAR_OREFLAG(huart);
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
