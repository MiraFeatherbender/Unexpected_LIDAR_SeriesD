## Plan: Implement REST GET Logic for RGB JSON (REST & RGB Modules)

This plan details how to implement the REST GET logic for retrieving RGB JSON data, using a struct with a buffer and semaphore for async notification, following the architectural decisions discussed.

### Steps
1. **Define the JSON request struct**  
   - Create a dedicated header file (e.g., `rest_context.h`) to define all shared REST/async context structs (such as `rgb_json_request_t` for JSON buffer, buffer size, length pointer, and semaphore).
   - Include this header in any module (e.g., REST, RGB, LIDAR) that needs to use or understand these structs.

2. **REST handler logic (wifi_http_server.c)**  
   - In the RGB GET handler, allocate and initialize the struct.
   - Create a semaphore and assign it to the struct.
   - Set the struct’s buffer pointers and pass its address in `msg.context`.
   - Send a dispatcher message to `TARGET_RGB` requesting JSON data.

3. **Wait for RGB response (REST side)**  
   - Wait on the semaphore with a timeout.
   - On success, send the JSON buffer as the HTTP response.
   - On timeout, send an error response.

4. **RGB module logic (io_rgb.c)**  
   - In the RGB dispatcher handler, check if `msg->context` is non-NULL and of the expected type.
   - Fill the provided buffer with the JSON data and set the length.
   - Signal the semaphore to notify the REST handler.

5. **Error handling and cleanup**  
   - Ensure the REST handler cleans up the semaphore and struct after use.
   - Handle all error cases (allocation, timeout, JSON generation).

### Further Considerations
1. Should the struct and logic be generic for other modules, or RGB-specific for now?
2. Consider buffer size and memory management for large/dynamic JSON.
3. Optionally, add logging for request/response timing and errors.
