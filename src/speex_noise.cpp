// src/speex_noise.cpp
#define PY_SSIZE_T_CLEAN
#ifndef Py_LIMITED_API
// Target Python 3.9 stable ABI (adjust if you set a different floor)
#define Py_LIMITED_API 0x03090000
#endif
#include <Python.h>

#include <speex_preprocess.h>

// -------------------- constants ------------------------------------
#define SAMPLES_10_MS 160
#define BYTES_10_MS (SAMPLES_10_MS * 2)

// -------------------- per-instance handle ----------------------------
struct SpeexNoiseHandle {
    SpeexPreprocessState *state;
};

static const char *CAPSULE_NAME = "speex_noise_cpp.SpeexNoiseHandle";

// -------------------- helpers --------------------
static void speex_noise_capsule_destructor(PyObject *capsule) {
    void *p = PyCapsule_GetPointer(capsule, CAPSULE_NAME);
    if (!p) {
        // Name mismatch or already invalid; do not raise from a destructor.
        PyErr_Clear();
        return;
    }
    auto *h = (SpeexNoiseHandle *)p;

    speex_preprocess_state_destroy(h->state);
    free(h);

    PyErr_Clear(); // ensure no lingering errors from any capsule calls
}

static inline SpeexNoiseHandle *get_handle(PyObject *cap) {
    return (SpeexNoiseHandle *)PyCapsule_GetPointer(cap, CAPSULE_NAME);
}

// -------------------- create_speex_noise() --------------------
static PyObject *mod_create_speex_noise(PyObject *, PyObject *args,
                                        PyObject *kwargs) {
    static const char *kwlist[] = {"auto_gain", "noise_suppression", NULL};
    double auto_gain;
    int noise_suppression;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "di", (char **)kwlist,
                                     &auto_gain, &noise_suppression)) {
        return NULL;
    }

    auto *h = (SpeexNoiseHandle *)malloc(sizeof(SpeexNoiseHandle));
    if (!h) {
        return PyErr_NoMemory();
    }

    h->state = speex_preprocess_state_init(SAMPLES_10_MS, 16000);
    int noise_state = (noise_suppression == 0) ? 0 : 1;
    speex_preprocess_ctl(h->state, SPEEX_PREPROCESS_SET_DENOISE, &noise_state);

    if (noise_suppression != 0) {
        speex_preprocess_ctl(h->state, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS,
                             &noise_suppression);
    }

    int agc_enabled = (auto_gain > 0) ? 1 : 0;
    speex_preprocess_ctl(h->state, SPEEX_PREPROCESS_SET_AGC, &agc_enabled);
    if (auto_gain > 0) {
        speex_preprocess_ctl(h->state, SPEEX_PREPROCESS_SET_AGC_LEVEL,
                             &auto_gain);
    }

    PyObject *capsule =
        PyCapsule_New((void *)h, CAPSULE_NAME, speex_noise_capsule_destructor);
    if (!capsule) {
        speex_preprocess_state_destroy(h->state);
        free(h);
        return NULL;
    }
    return capsule;
}

// -------------------- process_10ms(speex_noise, audio) --------------------
static PyObject *mod_process_10ms(PyObject *, PyObject *args,
                                  PyObject *kwargs) {
    static const char *kwlist[] = {"speex_noise", "audio", NULL};
    PyObject *cap = NULL;
    const char *data = nullptr;
    Py_ssize_t len = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Oy#", (char **)kwlist, &cap,
                                     &data, &len)) {
        return NULL;
    }

    SpeexNoiseHandle *h = get_handle(cap);
    if (!h) {
        // PyCapsule_GetPointer already set an error (name mismatch or NULL)
        return NULL;
    }

    PyObject *output = PyBytes_FromStringAndSize(NULL, BYTES_10_MS);
    if (!output) {
        return NULL;
    }

    const int16_t *samples = (const int16_t *)data;
    int16_t *output_samples =
        reinterpret_cast<int16_t *>(PyBytes_AsString(output));

    Py_BEGIN_ALLOW_THREADS memcpy(output_samples, samples, BYTES_10_MS);
    speex_preprocess_run(h->state, output_samples);
    Py_END_ALLOW_THREADS

        return output;
}

// -------------------- module boilerplate --------------------
static PyMethodDef module_methods[] = {
    {"create_speex_noise", (PyCFunction)mod_create_speex_noise,
     METH_VARARGS | METH_KEYWORDS,
     "create_speex_noise(auto_gain: float, noise_suppression: int) -> capsule"},
    {"process_10ms", (PyCFunction)mod_process_10ms,
     METH_VARARGS | METH_KEYWORDS,
     "process_10ms(speex_noise, audio: bytes) -> bytes"},
    {NULL, NULL, 0, NULL}};

static struct PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT,
    "speex_noise_cpp",
    "Noise suppression and automatic gain with speex",
    -1,
    module_methods,
    NULL,
    NULL,
    NULL,
    NULL};

PyMODINIT_FUNC PyInit_speex_noise_cpp(void) {
    PyObject *m = PyModule_Create(&module_def);
    if (!m) {
        return NULL;
    }

#ifdef VERSION_INFO
    PyObject *ver = PyUnicode_FromString(VERSION_INFO);
#else
    PyObject *ver = PyUnicode_FromString("dev");
#endif
    if (ver) {
        PyModule_AddObject(m, "__version__", ver); // steals ref
    }
    return m;
}
