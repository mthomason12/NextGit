/* glkstart.h: Unix-specific header file for NextGlk.
 * 
 * This header defines the Unix startup interface expected by 
 * programs linked with NextGlk (notably Git). It mirrors the
 * interface from CheapGlk/GlkTerm/XGlk.
 *
 * See docs/integration-attempt-1.md §3a for the required symbols.
 */

#ifndef NEXTGLK_START_H
#define NEXTGLK_START_H

/* We define our own TRUE and FALSE and NULL, because ANSI
 * is a strange world. */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

#define glkunix_arg_End (0)
#define glkunix_arg_ValueFollows (1)
#define glkunix_arg_NoValue (2)
#define glkunix_arg_ValueCanFollow (3)
#define glkunix_arg_NumberValue (4)

typedef struct glkunix_argumentlist_struct {
    char *name;
    int argtype;
    char *desc;
} glkunix_argumentlist_t;

typedef struct glkunix_startup_struct {
    int argc;
    char **argv;
} glkunix_startup_t;

extern glkunix_argumentlist_t glkunix_arguments[];

extern int glkunix_startup_code(glkunix_startup_t *data);

extern void glkunix_set_base_file(char *filename);
extern strid_t glkunix_stream_open_pathname_gen(char *pathname, 
    glui32 writemode, glui32 textmode, glui32 rock);
extern strid_t glkunix_stream_open_pathname(char *pathname, glui32 textmode, 
    glui32 rock);

#endif /* NEXTGLK_START_H */