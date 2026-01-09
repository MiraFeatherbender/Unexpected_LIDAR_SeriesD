## Plan: Incremental ESP32 Web + FATFS Expansion

Build your system in small, testable steps, ensuring each feature works before adding the next.

### Steps
1. **Refactor FATFS Access**
   - Create a dedicated file operations module (read, write, list, delete).
   - Update USB MSC and any existing code to use this module.

2. **Add Basic HTTP Server**
   - Serve static files (HTML/JS) from FATFS.
   - Test: Upload files to FATFS (via MSC or serial) and access them in a browser.

3. **Implement HTTP File Upload**
   - Add HTTP POST/PUT endpoint for uploading files to FATFS.
   - Test: Upload new web files via browser and verify they are served.

4. **Add REST API Endpoints**
   - Implement simple endpoints for sensor data and commands.
   - Test: Fetch sensor data and send commands from browser JS.

5. **Integrate WebSocket Server**
   - Add WebSocket support for live sensor data and command feedback.
   - Test: Live updates in browser using JavaScript.

6. **OTA Update Support**
   - Add OTA upload and update logic, using FATFS for image storage.
   - Test: Upload and apply firmware updates via web interface.

7. **Expand File Ops Usage**
   - Refactor other modules (e.g., noise/BMP) to use the file ops module.

### Further Considerations
1. Start with the simplest, most reusable modules (FATFS/file ops).
2. Test each feature in isolation before combining.
3. Use browser dev tools and local web dev for rapid UI iteration.
