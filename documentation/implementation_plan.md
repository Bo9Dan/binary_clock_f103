# План нового бінарного годинника STM32F103C8T6

## 1. Мета

Побудувати тестовий бінарний годинник на `STM32F103C8T6`.

Основні компоненти:

- `STM32F103C8T6` - мікроконтролер.
- `DS3231` - зовнішній RTC для збереження часу, дати і будильника.
- `3 x 74HC595` - каскад регістрів зсуву для LED.
- `APDS-9960` - жести та ambient light для автояскравості.
- `HTU21D` - температура і вологість.
- Active low-level buzzer - сигнал будильника.
- 4 кнопки - керування режимами і налаштуваннями.

Проєкт створюється через STM32CubeMX, збирається у VSCode через CMake.

## 2. Поточна CubeMX-конфігурація

Проєкт:

- MCU: `STM32F103C8Tx`.
- Toolchain / IDE: `CMake`.
- Clock: `HSI 8 MHz`, без PLL.
- Debug: `Serial Wire`.
- Timebase: `SysTick`.
- Internal STM32 RTC не використовується.

Периферія:

- `I2C1`
  - `PB6 = I2C1_SCL`
  - `PB7 = I2C1_SDA`
  - `100 kHz`
  - модулі: `DS3231`, `HTU21D`, `APDS-9960`

- `SPI1`
  - `PA5 = SPI1_SCK / CLK_595`
  - `PA7 = SPI1_MOSI / DATA_595`
  - `PA4 = GPIO_Output / LATCH_595`
  - режим: `Transmit Only Master`
  - SPI mode 0: `CPOL Low`, `CPHA 1 Edge`
  - `MSB First`

- `TIM3 PWM`
  - `PB0 = TIM3_CH3 / OE_595_PWM`
  - `Prescaler = 7`
  - `Period = 999`
  - частота PWM приблизно `1 kHz` при `8 MHz`
  - `OE_595` active-low, тому логіка яскравості буде інвертована в коді

- `TIM2`
  - app scheduler tick
  - `Prescaler = 7999`
  - `Period = 9`
  - interrupt кожні `10 ms`

- `USART1`
  - `PA9 = USART1_TX`
  - `PA10 = USART1_RX`
  - `115200 8N1`
  - debug logs

- Кнопки:
  - `PA0 = BTN_MODE`, pull-up
  - `PA1 = BTN_PLUS`, pull-up
  - `PA2 = BTN_MINUS`, pull-up
  - `PA3 = BTN_OK`, pull-up
  - натиснута кнопка читається як `GPIO_PIN_RESET`

- Interrupt pins:
  - `PB12 = DS3231_INT`, EXTI falling edge, pull-up
  - `PB14 = APDS_INT`, EXTI falling edge, pull-up
  - `EXTI line[15:10] interrupts` enabled

- Output pins:
  - `PB15 = BUZZER`, initial high, active-low buzzer
  - `PC13 = BOARD_LED`, initial high, service/debug LED

## 3. LED-дисплей

Використовуються три `74HC595` у каскаді.

Підключення до STM32:

- `SPI1_MOSI / PA7` -> `DS/SER` першого `74HC595`
- `SPI1_SCK / PA5` -> `SH_CP/SRCLK` всіх `74HC595`
- `LATCH_595 / PA4` -> `ST_CP/RCLK` всіх `74HC595`
- `Q7S/QH'` першого -> `DS/SER` другого
- `Q7S/QH'` другого -> `DS/SER` третього
- `OE` всіх регістрів -> `PB0 / TIM3_CH3`
- `MR/SRCLR` всіх регістрів -> `3.3V`

Відображення:

- 2 рядки по 7 LED.
- Верхній рядок показує перше значення.
- Нижній рядок показує друге значення.
- Третій регістр використовується для mode LEDs, alarm LED, status/error LEDs.

Режими:

- Time:
  - верхній рядок = години `0..23`
  - нижній рядок = хвилини `0..59`

- Date:
  - верхній рядок = місяць `1..12`
  - нижній рядок = день `1..31`

- Environment:
  - верхній рядок = температура у цілих градусах C
  - нижній рядок = вологість у цілих `%RH`

- Alarm:
  - верхній рядок = години будильника
  - нижній рядок = хвилини будильника

## 4. RTC, дата і будильник

Час і дата зберігаються в `DS3231`.

Принцип:

- STM32 не зберігає час як головне джерело правди.
- `TIM2` не рахує годинник, а тільки дає системний tick.
- RTC читається:
  - при старті
  - далі приблизно раз на секунду
- RTC записується тільки після підтвердження користувачем у setup mode.

Будильник:

- Реалізується через alarm-регістри `DS3231`.
- `DS3231 INT/SQW` підключений до `PB12`.
- Коли будильник спрацьовує:
  - DS3231 тягне `INT` вниз
  - STM32 отримує EXTI interrupt
  - код виставляє flag
  - основний loop вмикає buzzer і blink alarm LED
- Вимкнення будильника:
  - жест APDS-9960
  - або кнопка `OK`
- Після вимкнення потрібно очистити alarm flag у DS3231.

## 5. User Interface

Кнопки:

- `MODE`
- `PLUS`
- `MINUS`
- `OK`

Normal mode:

- `MODE`: перемикає `Time -> Date -> Environment -> Alarm`.
- `OK`: якщо будильник дзвонить, вимикає його.
- `OK` на Alarm screen: вмикає/вимикає будильник.
- long `OK`: входить у setup поточного режиму.

Setup mode:

- `PLUS/MINUS`: змінити поточне поле.
- short `OK`: перейти до наступного поля.
- long `OK`: зберегти і вийти.
- `MODE`: скасувати і вийти.

Setup fields:

- Time: `hours -> minutes`.
- Date: `month -> day -> year`.
- Alarm: `hours -> minutes -> enabled`.

## 6. Low Power

Основна стратегія для першої версії:

- `Sleep mode` через `WFI`.
- MCU прокидається від:
  - `TIM2` scheduler tick
  - `DS3231_INT`
  - `APDS_INT`
  - кнопок, якщо пізніше переведемо їх на EXTI

Причина:

- Sleep mode сумісний із PWM, SysTick/TIM, I2C, SPI і простий для debug.
- Stop/Standby mode розглянути пізніше, коли базова логіка стабільна.

## 7. Auto Brightness

Ручного налаштування яскравості не буде.

Принцип:

- `APDS-9960` читає ambient light.
- `brightness_service` переводить освітленість у PWM duty.
- `TIM3_CH3` керує `OE_595`.
- Оскільки `OE` active-low:
  - більше HIGH на `OE` = LED темніші
  - більше LOW на `OE` = LED яскравіші

Яскравість має змінюватися плавно.

## 8. Поточний bring-up код

Перший тест уже доданий у `Core/Src/main.c`.

Очікувана поведінка:

- `BOARD_LED / PC13` блимає.
- `TIM3 PWM` запускається для `OE_595`.
- Через `SPI1` кожні `150 ms` передається 24-бітний патерн.
- Один активний біт біжить по 24 виходах трьох `74HC595`.
- Через `USART1` відправляється стартове повідомлення.

Цей тест потрібен тільки для першої перевірки плати, SPI і 74HC595.

## 9. Наступні кроки

1. Стабільно прошити STM32F103 через ST-LINK.
2. Перевірити, що `BOARD_LED` блимає.
3. Підключити один `74HC595` без сенсорів і buzzer.
4. Перевірити SPI-патерн на LED через резистори.
5. Додати другий і третій `74HC595`.
6. Зафіксувати реальний порядок LED у mapping.
7. Винести bring-up код у модулі:
   - `shift_register_595`
   - `display_driver`
   - `display_encoder`
8. Додати `TIM2` scheduler callback.
9. Додати button debounce і перший UI.
10. Додати I2C scan.
11. Додати DS3231 read/write.
12. Додати DS3231 alarm registers.
13. Додати HTU21D.
14. Додати APDS gesture і ambient light.
15. Додати sleep mode через `WFI`.

## 10. Важливі правила підключення

- Не підключати `5V` до GPIO STM32.
- Для STM32F103 GPIO використовувати `3.3V` логіку.
- Усі LED підключати тільки через резистори.
- Якщо плата гріється або ST-LINK пищить, одразу вимкнути живлення.
- Для SWD потрібні:
  - `SWDIO -> PA13`
  - `SWCLK -> PA14`
  - `GND -> GND`
- Якщо плата живиться окремо, `3.3V` від ST-LINK не підключати.

