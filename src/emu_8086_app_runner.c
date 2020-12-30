#include <assembler.h>
#include <emu8086.h>
#include <emu_8086_app_runner.h>

extern struct instruction *_instruction_list;
extern struct instruction *_current_instruction, *_first_instruction;
extern struct label *label_list, *explore;
extern struct errors_list *first_err, *list_err;
extern int errors, assembler_step;

G_DEFINE_TYPE_WITH_PRIVATE(Emu8086AppCodeRunner, emu_8086_app_code_runner, G_TYPE_OBJECT);

enum
{
    EXEC_INS,
    ERROR_OCCURRED,
    INTERRUPT,
    EXEC_STOPPED,
    // PORT_STATE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

static void emu_8086_app_code_runner_init(Emu8086AppCodeRunner *runner);

static void emu_8086_app_code_runner_class_init(Emu8086AppCodeRunnerClass *klass);

static void emu_8086_app_code_runner_set_property(GObject *object,
                                                  guint property_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void
emu_8086_app_code_runner_get_property(GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec);

static void emu_8086_app_code_runner_set_property(GObject *object,
                                                  guint property_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec)
{
    Emu8086AppCodeRunner *self = EMU_8086_APP_CODE_RUNNER(object);
    // g_print("l %d\n", *value);

    switch ((Emu8086AppCodeRunnerProperty)property_id)
    {

    case PROP_RUNNER_FNAME:
        // *v = (gboolean *)value;

        self->priv->fname = g_value_get_string(value);
        // emu8086_win_change_theme(self);
        // g_print("filename: %s\n", self->filename);
        break;
    case PROP_RUNNER_CAN_RUN:
        self->priv->can_run = g_value_get_boolean(value);
        break;
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
emu_8086_app_code_runner_get_property(GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    Emu8086AppCodeRunner *self = EMU_8086_APP_CODE_RUNNER(object);

    switch ((Emu8086AppCodeRunnerProperty)property_id)
    {
    case PROP_RUNNER_FNAME:
        g_value_set_string(value, self->priv->fname);
        break;
    case PROP_RUNNER_CAN_RUN:
        g_value_set_boolean(value, self->priv->can_run);
        break;
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

Emu8086AppCodeRunner *emu_8086_app_code_runner_new(gchar *fname, gboolean can_run)
{
    return g_object_new(EMU_8086_APP_CODE_RUNNER_TYPE,
                        "filename", fname,
                        "can_run", can_run,
                        NULL);
};

static void emu_8086_app_code_runner_exec_ins(Emu8086AppCodeRunner *runner)
{
    PRIV_CODE_RUNNER;
    priv->ie++;
}

static void emu_8086_app_code_runner_error_occurred(Emu8086AppCodeRunner *runner)
{
    PRIV_CODE_RUNNER;
    priv->ie = 0;
}

static void emu_8086_app_code_runner_interrupt(Emu8086AppCodeRunner *runner)
{
    PRIV_CODE_RUNNER;
    priv->ie++;
}

static void emu_8086_app_code_runner_exec_stopped(Emu8086AppCodeRunner *runner)
{
    PRIV_CODE_RUNNER;
    priv->ie = 0;

    priv->em = NULL;
}

static void emu_8086_app_code_runner_class_init(Emu8086AppCodeRunnerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->set_property = emu_8086_app_code_runner_set_property;
    object_class->get_property = emu_8086_app_code_runner_get_property;

    klass->exec_ins = emu_8086_app_code_runner_exec_ins;
    klass->exec_stopped = emu_8086_app_code_runner_exec_stopped;

    g_object_class_install_property(object_class,
                                    PROP_RUNNER_FNAME,
                                    g_param_spec_string("filename",
                                                        "Filename",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));

    g_object_class_install_property(object_class,
                                    PROP_RUNNER_CAN_RUN,
                                    g_param_spec_boolean("can_run",
                                                         "Can Run",
                                                         "The runners' text window type",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

    signals[EXEC_STOPPED] = g_signal_new("exec_stopped", G_TYPE_FROM_CLASS(klass),
                                         G_SIGNAL_RUN_FIRST,
                                         G_STRUCT_OFFSET(Emu8086AppCodeRunnerClass, exec_stopped),
                                         NULL, NULL, NULL,
                                         G_TYPE_NONE, 0);
    signals[EXEC_INS] = g_signal_new("exec_ins", G_TYPE_FROM_CLASS(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     G_STRUCT_OFFSET(Emu8086AppCodeRunnerClass, exec_ins),
                                     NULL, NULL, NULL,
                                     G_TYPE_NONE, 0);

    signals[ERROR_OCCURRED] = g_signal_new("error_occured", G_TYPE_FROM_CLASS(klass),
                                           G_SIGNAL_RUN_FIRST,
                                           G_STRUCT_OFFSET(Emu8086AppCodeRunnerClass, error_occurred),
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           0);

    signals[INTERRUPT] = g_signal_new("interrupt", G_TYPE_FROM_CLASS(klass),
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET(Emu8086AppCodeRunnerClass, interrupt),
                                      NULL, NULL, NULL,
                                      G_TYPE_NONE, 0);
}

static void emu_8086_app_code_runner_init(Emu8086AppCodeRunner *runner)
{
    runner->priv = emu_8086_app_code_runner_get_instance_private(runner);
    runner->priv->aCPU = NULL;
    runner->priv->fname = NULL;
    runner->priv->ie = 0;
    runner->priv->state = STOPPED;
}

static void emu_free(Emu8086AppCodeRunner *runner)
{
    PRIV_CODE_RUNNER;
    struct emu8086 *aCPU = priv->aCPU;
    if (aCPU == NULL)
        return;
    struct instruction *_current_instruction2 = _first_instruction;

    struct instruction *prev = _current_instruction2->prev;
    struct instruction *next;
    next = _current_instruction2;
    //free(next);
    int i;
    i = 0;
    // printf("%d\n", label_identifier);
    while (next != NULL)
    {
        //  printf("jj\n");
        _current_instruction2 = next;

        next = next->next;
        free(_current_instruction2);
        //   }
        //}
        i++;
        //break;
    };
    i = 0;
    // g_print("herennn 00 \n\n ");/
    struct label *explore = label_list, *_next;
    if (label_list != NULL)
    {
        while (explore->left != NULL)
            explore = explore->left;
        _next = explore;
        while (_next != NULL)
        {
            explore = _next;
            _next = _next->right;
            free(explore);
            i++;
        }
    }
    // g_print("herennn \n\n ");
    if ((errors > 0) && (first_err != NULL))
    {
        struct errors_list *e = first_err, *n;
        n = first_err;
        if (first_err->next == NULL)
        {
            // free(first_err->message);
            // first_err->message = NULL;
            free(first_err);
        }
        else
        {
            while (n != NULL)
            {
                e = n;
                n = n->next;
                // free(e->message);

                free(e);
                // i++;
            }
        }
    }
    first_err = NULL;
    label_list = NULL;
    _instruction_list = NULL;
    errors = 0;
    _first_instruction = NULL;
    aCPU->instructions_list = NULL;
    list_err = NULL;

    //  free(aCPU->mSFR);
    // free(aCPU->mDataMem);

    free(aCPU);
    set_app_state(runner, STOPPED);
    priv->aCPU = NULL;
    // g_free(priv->fname);
    // priv->fname = NULL;
}

void set_app_state(Emu8086AppCodeRunner *runner, gint state)
{
    PRIV_CODE_RUNNER;
    priv->state = state;
}

void execute(struct emu8086 *aCPU)
{

    if (IP < aCPU->end_address - 1)
    {
        int op = *(CODE_SEGMENT_IP), handled = 0;
        aCPU->op[op](aCPU, &handled);
        if (!handled)
        {
            char buf[15];
            sprintf(buf, "Unhandled instrution on line %d, opcode: %x", _INSTRUCTIONS->line_number, op); //message()
            message(buf, ERR, _INSTRUCTIONS->line_number);
        }
        if (aCPU->skip_next)
            aCPU->skip_next = 0;
        else if (_INSTRUCTIONS->next != NULL)
            _INSTRUCTIONS = _INSTRUCTIONS->next;
        if (IP > 0xffff)
        {
            IP = IP - 0xffff;
            CS = CS - 0x10000;
        }
        handled = 0;
    }
}

void stop(Emu8086AppCodeRunner *runner, gboolean reset)
{
    PRIV_CODE_RUNNER;
    g_return_if_fail(priv->state != STOPPED);

    if (priv->aCPU != NULL)
        emu_free(runner);
    set_app_state(runner, STOPPED);
    if (reset)
    {
        g_signal_emit(runner, signals[EXEC_STOPPED], 0);
    }
}

void stop_clicked_app(Emu8086AppCodeRunner *runner)
{
    stop(runner, TRUE);
}

int emu_run(Emu8086AppCodeRunner *runner)
{
    PRIV_CODE_RUNNER;
    // g_return_if_fail()
    if (priv == NULL)
        return;
    if (priv->state != PLAYING)
        return 0;
    struct emu8086 *aCPU = priv->aCPU;
    g_return_val_if_fail(aCPU != NULL, 0);
    g_return_val_if_fail(aCPU->instructions_list != NULL, 0);
    //struct emu8086 *aCPU = priv->aCPU;
    if (aCPU->is_first == 1)
    {
        g_signal_emit(runner, signals[EXEC_INS], 0);
        ;
        aCPU->is_first = 0;
        return 1;
    }
    errors = 0;
    execute(aCPU);
    if (errors > 0)
    {

        if (list_err != NULL)
        {

            if (priv->em != NULL)
                g_free(priv->em);
            priv->em = NULL;
            priv->em = g_strdup(list_err->message);
            g_signal_emit(runner, signals[ERROR_OCCURRED], 0);
        }
        // emu_8086_app_window_flash(priv->win, first_err->message);
        g_debug(list_err->message);
        stop(runner, FALSE);
        return 0;
        // exit(1);
    }
    g_signal_emit(runner, signals[EXEC_INS], 0);

    // if (priv->win != NULL)
    //     emu_8086_app_window_update_wids(priv->win, aCPU);
    if (IP == aCPU->end_address - 1)
    {
        // g_timeout_
        stop(runner, TRUE);
        return 0;
    }
    return 1;
}
static int emu_init(Emu8086AppCodeRunner *runner)
{
    PRIV_CODE_RUNNER;
    struct emu8086 *aCPU = priv->aCPU;
    gchar *fname;
    fname = priv->fname;
    if (aCPU == NULL)
    {
        aCPU = emu8086_new();
        priv->aCPU = aCPU;

        if (aCPU == NULL)
            exit(1);
    }
    errors = 0;
    assembler_step = 0;
    do_assembly(priv->aCPU, fname);
    if (errors > 0)
    {
        // g_print(first_err->message);
        g_print("runn: 390\n");

        // emu_8086_app_window_flash(priv->win, first_err->message);
        set_app_state(runner, STOPPED);
        emu_free(runner);
        return 0;
    }
    assembler_step = 1;
    errors = 0;
    do_assembly(priv->aCPU, fname);
    if (errors > 0)
    {

        if (list_err != NULL)
        {

            if (priv->em != NULL)
                g_free(priv->em);
            priv->em = NULL;
            priv->em = g_strdup(list_err->message);
            g_signal_emit(runner, signals[ERROR_OCCURRED], 0);
        }
        // emu_8086_app_window_flash(priv->win, first_err->message);
        set_app_state(runner, STOPPED);
        emu_free(runner);
        return 0;
    }

    CS = aCPU->code_start_addr / 0x10;
    DS = 0x03ff;
    BX = 5;
    BP = 15;
    SP = 128;
    _SS = ((CS * 0x10) - 0x20000) / 0x10;
    //  g_print("knnkk\n");
    return 1;
}

void step_clicked_app(Emu8086AppCodeRunner *runner)
{
    PRIV_CODE_RUNNER;
    g_return_if_fail(priv->state != PLAYING);

    set_app_state(runner, STEP);
    if (priv->state != STEP)
        return;
    if (priv->aCPU == NULL)
    {
        g_print("runner: 431\n");
        if (!emu_init(runner))
            return;
    }
    if (errors > 0)
    {
        if (!emu_init(runner))
            return;
    }
    if (priv->aCPU != NULL)
    {
        struct emu8086 *aCPU = priv->aCPU;
        if (aCPU->is_first == 1)
        {
            g_signal_emit(runner, signals[EXEC_INS], 0);
            ;
            // emu_8086_app_window_update_wids(priv->win, priv->aCPU);
            aCPU->is_first = 0;
            return;
        }
        // emu_8086_app_window_update_wids(priv->win, priv->aCPU);
        errors = 0; // g_print("kkk\n");
        execute(priv->aCPU);
        if (errors > 0)
        {

            if (list_err != NULL)
            {

                if (priv->em != NULL)
                    g_free(priv->em);
                priv->em = NULL;
                priv->em = g_strdup(list_err->message);
                g_signal_emit(runner, signals[ERROR_OCCURRED], 0);
            }
            // emu_8086_app_window_flash(priv->win, first_err->message);
            g_debug(list_err->message);
            stop(runner, FALSE);
            return;
            // exit(1);
        }
        g_signal_emit(runner, signals[EXEC_INS], 0);
        ;
        //  emu_8086_app_window_update_wids(priv->win, priv->aCPU);
    }
}
void step_over(Emu8086AppCodeRunner *runner, uint16_t be)
{
    PRIV_CODE_RUNNER;
    struct emu8086 *aCPU = priv->aCPU;

    if (aCPU != NULL)
    {

        aCPU->last_ip = be;
        while (IP == aCPU->last_ip && IP < aCPU->end_address - 1)

        {

            errors = 0;
            execute(priv->aCPU);
            if (errors > 0)
            {

                if (list_err != NULL)
                {

                    if (priv->em != NULL)
                        g_free(priv->em);
                    priv->em = NULL;
                    priv->em = g_strdup(list_err->message);
                    g_signal_emit(runner, signals[ERROR_OCCURRED], 0);
                }
                // emu_8086_app_window_flash(priv->win, first_err->message);
                g_debug(list_err->message);
                stop(runner, FALSE);
                return;
                // exit(1);
            }
            // emu_8086_app_window_update_wids(priv->win, priv->aCPU);
        }
        g_signal_emit(runner, signals[EXEC_INS], 0);
        ;
    }
}

void step_over_clicked_app(Emu8086AppCodeRunner *runner)
{
    PRIV_CODE_RUNNER;
    struct emu8086 *aCPU = NULL;
    aCPU = priv->aCPU;

    g_return_if_fail(priv->state != PLAYING);

    if (aCPU == NULL)
    {
        if (!emu_init(runner))
            return;
        aCPU = priv->aCPU;
    }
    if (errors > 0)
    {
        if (!emu_init(runner))
            return;
        aCPU = priv->aCPU;
    }

    set_app_state(runner, STEP);
    if (priv->state != STEP)
        return;
    if (aCPU == NULL)
    {
        return;
    }
    if (aCPU->is_first == 1)
    {
        g_signal_emit(runner, signals[EXEC_INS], 0);
        ;
        // emu_8086_app_window_update_wids(priv->win, priv->aCPU);
        aCPU->is_first = 0;
        return;
    }
    errors = 0;
    step_over(runner, IP);
    if (errors > 0)
    {

        if (list_err != NULL)
        {

            if (priv->em != NULL)
                g_free(priv->em);
            priv->em = NULL;
            priv->em = g_strdup(list_err->message);
            g_signal_emit(runner, signals[ERROR_OCCURRED], 0);
        }
        g_debug(list_err->message);
        stop(runner, FALSE);
        return;
        // exit(1);
    }
}
void run_clicked_app(Emu8086AppCodeRunner *runner)
{
    PRIV_CODE_RUNNER;

    if (priv->state == PLAYING)
    {
        // emu_8086_app_window_flash(priv->win, "RUNNING");
        // g_signal_emit(runner, signals[EXEC_INS], 0);
        ;

        return;
    }
    struct emu8086 *aCPU = priv->aCPU;
    if (aCPU == NULL)
    {
        if (!emu_init(runner))
            return;
    }
    if (errors > 0)
    {
        if (!emu_init(runner))
            return;
    }
    if (priv->state == STEP)
    {
        set_app_state(runner, PLAYING);
        priv->to = g_timeout_add(1000, (GSourceFunc)emu_run, runner);
        return;
    }

    priv->to = g_timeout_add(1000, (GSourceFunc)emu_run, runner);
    set_app_state(runner, PLAYING);

    // g_timeout_add
} //x

void set_fname(Emu8086AppCodeRunner *runner, gchar *fname)
{
    PRIV_CODE_RUNNER;
    if (priv->fname != NULL)
        g_free(priv->fname);
    priv->fname = g_strdup(fname);
}
struct emu8086 *getCPU(Emu8086AppCodeRunner *runner)
{
    return runner->priv->aCPU;
}