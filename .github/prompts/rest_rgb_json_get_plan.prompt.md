## Plan: Implement REST GET Logic for RGB JSON (REST & RGB Modules)

This plan details how to implement the REST GET logic for retrieving RGB JSON data, using a struct with a buffer and semaphore for async notification, following the architectural decisions discussed.


### Steps
1. **Define the JSON request struct**
   - Create a dedicated header file (e.g., `rest_context.h`) to define all shared REST/async context structs (such as `rgb_json_request_t` for JSON buffer, buffer size, length pointer, and semaphore).
   - Document struct fields and intended usage for future modules.
   - Include this header in any module (e.g., REST, RGB, LIDAR) that needs to use or understand these structs.

2. **REST handler logic (wifi_http_server.c)**
   - Add the necessary #include for `rest_context.h`.
   - In the RGB GET handler, allocate and initialize the struct.
   - Create a semaphore and assign it to the struct.
   - Set the struct’s buffer pointers and pass its address in `msg.context`.
   - Send a dispatcher message to `TARGET_RGB` requesting JSON data.
   - Add logging for request initiation and struct allocation.

3. **Wait for RGB response (REST side)**
   - Wait on the semaphore with a timeout.
   - On success, send the JSON buffer as the HTTP response.
   - On timeout, send an error response.
   - Add logging for response timing and errors.


4. **RGB module logic (io_rgb.c)**
   - Add the necessary #include for `rest_context.h`.
   - Refactor the RGB dispatcher handler to recognize and process both REST GET requests (using the context struct to fill JSON) and REST command messages (e.g., color/animation changes).
   - For GET requests, check if `msg->context` is non-NULL and of the expected type, fill the buffer with JSON, set the length, and signal the semaphore.
   - For command messages, validate and apply incoming command data from REST, and add logging and error handling for command processing.
   - Add logging for JSON generation, notification, and command handling.

5. **Error handling and cleanup**
   - Ensure the REST handler cleans up the semaphore and struct after use.
   - Handle all error cases (allocation, timeout, JSON generation).
   - Add error logging and diagnostics for debugging.

6. **Testing and validation**
   - Write unit tests or integration tests for the REST GET logic and RGB JSON response.
   - Test with valid and invalid requests, large JSON payloads, and timeout scenarios.
   - Validate that the system remains responsive and robust under load.

7. **Documentation and future extensibility**
   - Document the workflow and struct usage in code comments and project docs.
   - Plan for extending the struct and logic for other modules (e.g., LIDAR, battery) as needed.
   - Review and update the plan as requirements evolve.

### Further Considerations
1. Should the struct and logic be generic for other modules, or RGB-specific for now?
2. Consider buffer size and memory management for large/dynamic JSON.
3. Optionally, add logging for request/response timing and errors.
