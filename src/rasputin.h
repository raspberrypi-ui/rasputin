
#include <gtk/gtk.h>


extern int dclick;
extern gboolean left_handed;
extern float accel;

extern int delay;
extern int interval;

extern char fstr[16];


extern GSettings *mouse_settings, *keyboard_settings;


typedef struct {
    void (*load_config) (void);
    void (*set_doubleclick) (void);
    void (*set_acceleration) (void);
    void (*set_keyboard) (void);
    void (*set_lefthanded) (void);
    void (*save_config) (void);
} km_functions_t;

extern char *update_facc_str (void);
