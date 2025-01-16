struct cwc_server;
struct cwc_input_manager;

extern void setup_output(struct cwc_server *s);
extern void cleanup_output(struct cwc_server *s);

extern void setup_xdg_shell(struct cwc_server *s);
extern void cleanup_xdg_shell(struct cwc_server *s);

extern void setup_decoration_manager(struct cwc_server *s);
extern void cleanup_decoration_manager(struct cwc_server *s);

extern void setup_layer_shell(struct cwc_server *s);
extern void cleanup_layer_shell(struct cwc_server *s);

extern void setup_cwc_session_lock(struct cwc_server *s);
extern void cleanup_cwc_session_lock(struct cwc_server *s);

extern void setup_pointer(struct cwc_input_manager *input_mgr);
extern void cleanup_pointer(struct cwc_input_manager *input_mgr);

extern void setup_keyboard(struct cwc_input_manager *input_mgr);
extern void cleanup_keyboard(struct cwc_input_manager *input_mgr);

void setup_seat(struct cwc_input_manager *input_mgr);
void cleanup_seat(struct cwc_input_manager *input_mgr);

extern void cwc_idle_init(struct cwc_server *s);
extern void cwc_idle_fini(struct cwc_server *s);
extern void xwayland_init(struct cwc_server *s);
extern void xwayland_fini(struct cwc_server *s);
