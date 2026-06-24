# STM32 HAL + MISRA-C 2025 Coding Style (Cheatsheet)

Bản tóm gọn các quy tắc cốt lõi, áp dụng cho lập trình C trên STM32 (HAL), tuân thủ C99 và MISRA-C 2025.

---

## 1. Naming Conventions (Bảng chính)

| Element | Rule | Ví dụ |
| :--- | :--- | :--- |
| **File, Func, Local Var, Static Var** | `snake_case` | `uart_driver.c`, `timeout_ms`, `rx_buffer` |
| **Macro, Enum value** | `UPPER_SNAKE_CASE` | `UART_BUFFER_SIZE`, `UART_STATE_IDLE` |
| **Struct, Enum, Typedef** | `PascalCase_t` | `UartConfig_t`, `UartState_t` |
| **HAL Handle pointer** | `h + lowercase` | `huart1`, `htim2` |
| **Private function** | `static snake_case` | `static uart_isr_handler(void)` |
| **Getter / Setter** | `get_ / set_` | `get_baudrate()`, `set_baudrate()` |
| **Boolean query** | `is_ / has_` | `is_tx_complete()`, `has_error()` |

---

## 2. Core Rules (Rút gọn)

### Variables
- **Cấm dùng biến toàn cục (Global)** (MISRA 8.8, 8.9). 
  - Ưu tiên dùng `static` + comment hoặc gói trong `struct`.
- **Cấm tiền tố** kiểu `g_`, `s_`, `p_`. Thay vào đó, đặt tên rõ nghĩa (VD: `uart_handle`, `state_out`).

### Pointers
- **Bắt buộc kiểm tra `NULL`** cho mọi tham số con trỏ trước khi dùng.
- **Cấm `typedef`** cho kiểu con trỏ (VD: không được viết `typedef uint8_t* BytePtr_t;`).

### Types (C99)
- Bắt buộc dùng `<stdint.h>` (`uint8_t`, `uint32_t`...), `<stdbool.h>` và `size_t`.
- Hằng số unsigned **bắt buộc** có hậu tố `U` (VD: `100U`, `0xFF00U`).

### Functions
- Độ dài hàm lý tưởng: **< 50 dòng**.
- Nên trả về `enum error code` thay vì `void` để xử lý lỗi rõ ràng.

### Comments
- Dùng Doxygen: `/** @param @return @note */`.
- Viết **WHY** (lý do), không viết **WHAT** (điều hiển nhiên như `i++`).

### Headers
- Có Include Guard và `extern "C"` cho C++.
- Thứ tự include: `Standard` → `HAL` → `Project`.
- **Cấm định nghĩa biến / khởi tạo trong `.h`** (chỉ để khai báo `extern` hoặc `static inline` ngắn).

---

## 3. 🚨 MISRA-C 2025 (3 Rules mới bắt buộc)

### Rule 8.18 - Cấm "Tentative definitions" trong Header
❌ **Sai**: `uint32_t uart_baud = 115200U;` (đặt trong file .h)  
✅ **Đúng**:
```c
// Cách 1: Khai báo extern trong .h, định nghĩa trong .c
// File .h
extern uint32_t uart_baud;
// File .c
uint32_t uart_baud = 115200U;

// Cách 2 (Tốt nhất): Dùng hàm getter static const
uint32_t get_uart_baud(void) {
    static const uint32_t baud = 115200U;
    return baud;
}
