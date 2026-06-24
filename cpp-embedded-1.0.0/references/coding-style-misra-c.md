# STM32 HAL + MISRA-C 2025 Coding Style (Cheatsheet)

A concise summary of core rules applied to C programming on STM32 (HAL), compliant with C99 and MISRA-C 2025.

---

## 1. Naming Conventions

| Element | Rule | Example |
| :--- | :--- | :--- |
| **File, Func, Local Var, Static Var** | `snake_case` | `uart_driver.c`, `timeout_ms`, `rx_buffer` |
| **Macro, Enum value** | `UPPER_SNAKE_CASE` | `UART_BUFFER_SIZE`, `UART_STATE_IDLE` |
| **Struct, Enum, Typedef** | `PascalCase_t` | `UartConfig_t`, `UartState_t` |
| **HAL Handle pointer** | `h` + lowercase | `huart1`, `htim2` |
| **Private function** | `static snake_case` | `static uart_isr_handler(void)` |
| **Getter / Setter** | `get_` / `set_` | `get_baudrate()`, `set_baudrate()` |
| **Boolean query** | `is_` / `has_` | `is_tx_complete()`, `has_error()` |

---

## 2. Core Rules (Abridged)

### Variables
- **Global variables are forbidden** (MISRA 8.8, 8.9).  
  - Prefer `static` with a comment, or encapsulate in a `struct`.
- **Prefixes** like `g_`, `s_`, `p_` are not allowed. Instead, use descriptive names (e.g., `uart_handle`, `state_out`).

### Pointers
- **NULL check is mandatory** for all pointer parameters before use.
- **Do not `typedef` pointer types** (e.g., do not write `typedef uint8_t* BytePtr_t;`).

### Types (C99)
- Always use `<stdint.h>` (`uint8_t`, `uint32_t`...), `<stdbool.h>`, and `size_t`.
- Unsigned constants **must** have the `U` suffix (e.g., `100U`, `0xFF00U`).

### Functions
- Ideal function length: **< 50 lines**.
- Prefer returning an `enum error code` over `void` for explicit error handling.

### Comments
- Use Doxygen: `/** @param @return @note */`.
- Write **WHY** (the reason), not **WHAT** (the obvious like `i++`).
- Write by English language

### Headers
- Include Guard and `extern "C"` for C++ compatibility are required.
- Include order: `Standard` → `HAL` → `Project`.
- **Defining variables or initializations in `.h` is forbidden** (only declarations, `extern`, or short `static inline` functions).

---

## 3. 🚨 MISRA-C 2025 (3 Mandatory New Rules)

### Rule 8.18 - No "Tentative definitions" in Headers
❌ **Wrong**: `uint32_t uart_baud = 115200U;` (placed in a .h file)  
✅ **Correct**:
```c
// Method 1: extern declaration in .h, definition in .c
// File .h
extern uint32_t uart_baud;
// File .c
uint32_t uart_baud = 115200U;

// Method 2 (Best): use a static const getter function
uint32_t get_uart_baud(void) {
    static const uint32_t baud = 115200U;
    return baud;
}