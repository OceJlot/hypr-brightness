#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>


// Get active workspace data from Hyprland socket
void get_active_window_data(char* buffer, size_t size) {
    char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!sig) {
        fprintf(stderr, "ERROR: HYPRLAND_INSTANCE_SIGNATURE is missing!\n");
        return;
    }
    if (!runtime_dir){
        fprintf(stderr, "ERROR: XDG_RUNTIME_DIR is missing!\n");
        return;
    }
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return;
    
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/hypr/%s/.socket.sock", runtime_dir, sig);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "ERROR: Connection failed: %s\n", strerror(errno));
        close(sock);
        return;
    }
    
    const char* cmd = "j/activeworkspace";
    if (write(sock, cmd, strlen(cmd)) < 0) {
        fprintf(stderr, "ERROR: Write failed\n");
        close(sock);
        return;
    }
    
    ssize_t n = read(sock, buffer, size - 1);
    if (n <= 0) {
        fprintf(stderr, "ERROR: Read failed or returned 0 bytes (Error: %s)\n", strerror(errno));
    } else {
        buffer[n] = '\0';
    }
    close(sock);
}

// Extract monitor name from active workspace data
int get_monitor_name(char* result_buffer, size_t max_len) {
    char response[127] = {0};
    get_active_window_data(response, sizeof(response));

    const char* keyword = "\"monitor\": \"";
    char* match_string = strstr(response, keyword);
    if (!match_string) return 0;
    char* start = match_string + strlen(keyword);
    char* end = strchr(start, '\"');
    if (!end) return 0;
    size_t length = end - start;
    if (length >= max_len) {
        fprintf(stderr, "ERROR: Monitor name is larger than buffer size!\n");
        return 0;
    }
    memcpy(result_buffer, start, length);
    result_buffer[length] = '\0';
    return 1;
}
