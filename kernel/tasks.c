extern void print_string(char *str);

void task_a() {
    while(1) {
        // Simple delay loop to slow down printing
        for(int i=0; i<10000000; i++);
        print_string("A");
    }
}

void task_b() {
    while(1) {
        // Simple delay loop
        for(int i=0; i<10000000; i++);
        print_string("B");
    }
}
