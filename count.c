// count.c
#include <stdio.h>

int main() {
    int lines = 0;
    char buffer[1024];
    while (fgets(buffer, 1024, stdin)) {
        lines++;
    }
    printf("%d\n", lines);
    return 0;
}