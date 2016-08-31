
#ifndef __scintilla_marshal_MARSHAL_H__
#define __scintilla_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* NONE:INT,OBJECT (scintilla-marshal.list:1) */
extern void scintilla_marshal_VOID__INT_OBJECT (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);
#define scintilla_marshal_NONE__INT_OBJECT	scintilla_marshal_VOID__INT_OBJECT

/* NONE:INT,BOXED (scintilla-marshal.list:2) */
extern void scintilla_marshal_VOID__INT_BOXED (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);
#define scintilla_marshal_NONE__INT_BOXED	scintilla_marshal_VOID__INT_BOXED

G_END_DECLS

#endif /* __scintilla_marshal_MARSHAL_H__ */

