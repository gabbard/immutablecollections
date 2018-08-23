#include <Python.h>
#include <structmember.h>
#include <setobject.h>

// based off of PVector https://github.com/tobgu/pyrsistent/blob/master/pvectorcmodule.c
static PyTypeObject ImmutableSetType;

// #define debug(...)
#define debug printf

typedef struct {
    PyObject_HEAD;
    // TODO: use a tuple instead of a list
    PyObject* orderList;
    PyObject* wrappedSet;
    //PyObject *in_weakreflist; /* List of weak references */
} ImmutableSet;

typedef struct {
    PyObject_HEAD;
    PyObject *orderList;
    PyObject *wrappedSet;
    PyObject *orderKey;
    //PyObject *in_weakreflist; /* List of weak references */
} ImmutableSetBuilder;

// this gets initialized in the module initialization code (moduleinit)
static ImmutableSet *EMPY_SET = NULL;

static ImmutableSet *makeEmptySet() {
    ImmutableSet *immutableset = PyObject_GC_New(ImmutableSet, &ImmutableSetType);
    PyObject_GC_Track((PyObject *) immutableset);
    immutableset->orderList = PyList_New(0);
    immutableset->wrappedSet = PySet_New(NULL);
    return immutableset;
}

// pre-declare some functions
static ImmutableSetBuilder *immutablecollections_immutablesetbuilder_internal();

static ImmutableSetBuilder *immutablecollections_immutablesetbuilder(
        PyObject *self, PyObject *args, PyObject *keywords);

static ImmutableSet *ImmutableSetBuilder_build(ImmutableSetBuilder *self);

static ImmutableSetBuilder *ImmutableSetBuilder_add_all(ImmutableSetBuilder *self, PyObject *args);

static ImmutableSetBuilder *ImmutableSetBuilder_add(ImmutableSetBuilder *self, PyObject *item);


#define HANDLE_ITERATION_ERROR()                         \
    if (PyErr_Occurred()) {                              \
      if (PyErr_ExceptionMatches(PyExc_StopIteration)) { \
        PyErr_Clear();                                   \
      } else {                                           \
        return NULL;                                     \
      }                                                  \
    }

// No access to internal members
static PyMemberDef ImmutableSet_members[] = {
        {NULL}  /* Sentinel */
};

static PyMemberDef ImmutableSetBuilder_members[] = {
        {NULL}  /* Sentinel */
};

static void ImmutableSet_dealloc(ImmutableSet *self) {
    debug("In dealloc\n");
    //PyObject_ClearWeakRefs((PyObject *) self);
    debug("post clear weakrefs\n");

    PyObject_GC_UnTrack((PyObject*)self);
    Py_TRASHCAN_SAFE_BEGIN(self);

            Py_CLEAR(self->orderList);
            Py_CLEAR(self->wrappedSet);
//    PyMem_Free(self->orderList);
//    PyMem_Free(self->wrappedSet);

    PyObject_GC_Del(self);
    Py_TRASHCAN_SAFE_END(self);
    debug("end dealloc\n");
}

static void ImmutableSetBuilder_dealloc(ImmutableSetBuilder *self) {
    debug("In builder dealloc\n");
    //PyObject_ClearWeakRefs((PyObject *) self);
    debug("post clear weakrefs builder\n");

    PyObject_GC_UnTrack((PyObject *) self);
    Py_TRASHCAN_SAFE_BEGIN(self);

            Py_CLEAR(self->orderList);
            Py_CLEAR(self->wrappedSet);
            //PyMem_Free(self->orderList);
            //PyMem_Free(self->wrappedSet);

            PyObject_GC_Del(self);
            Py_TRASHCAN_SAFE_END(self);
    debug("end builder dealloc\n");
}


static long ImmutableSet_hash(ImmutableSet* self) {
    return PyObject_Hash(self->wrappedSet);
}

static Py_ssize_t ImmutableSet_len(ImmutableSet* self) {
    return PyList_Size(self->orderList);
}

static int ImmutableSet_contains(ImmutableSet* self, PyObject* queryItem) {
    return PySet_Contains(self->wrappedSet, queryItem);
}

static PyObject* ImmutableSet_get_item(ImmutableSet *self, Py_ssize_t pos) {
    return PyList_GetItem(self->orderList, pos);
}

static PySequenceMethods ImmutableSet_sequence_methods = {
        (lenfunc)ImmutableSet_len,            /* sq_length */
        NULL,      /* sq_concat - n/a (immutable) */
        NULL /*(ssizeargfunc)PVector_repeat*/,    /* sq_repeat */
        (ssizeargfunc)ImmutableSet_get_item,  /* sq_item */
        // TODO: should we support slicing?
        NULL,                            /* sq_slice */
        NULL,                            /* sq_ass_item */
        NULL,                            /* sq_ass_slice */
        (objobjproc)ImmutableSet_contains, /* sq_contains */
        NULL,                            /* sq_inplace_concat */
        NULL,                            /* sq_inplace_repeat */
};

static ImmutableSet *ImmutableSet_union(PyObject *self, PyObject *other) {
    // inefficient placeholder implementation
    ImmutableSetBuilder *builder = immutablecollections_immutablesetbuilder_internal();

    ImmutableSetBuilder_add_all(builder, self);
    ImmutableSetBuilder_add_all(builder, other);

    return ImmutableSetBuilder_build(builder);
}

static ImmutableSet *ImmutableSet_intersection(PyObject *self, PyObject *other) {
    // inefficient placeholder implementation

    // TODO: check that other is set_like, after we sub-class ourselves from AbstractSet

    ImmutableSetBuilder *builder = immutablecollections_immutablesetbuilder_internal();

    PyObject *it;
    it = PyObject_GetIter(self);
    if (it == NULL) {
        return NULL;
    }

    PyObject *(*iternext)(PyObject *);
    iternext = *Py_TYPE(it)->tp_iternext;
    PyObject *item = iternext(it);

    while (item != NULL) {
        if (PySet_Contains(other, item)) {
            ImmutableSetBuilder_add(builder, item);
        }
        item = iternext(it);
    }

    return ImmutableSetBuilder_build(builder);
}

static PyMethodDef ImmutableSet_methods[] = {
        {"union",        (PyCFunction) ImmutableSet_union,        METH_O,
                "Gets the union of this set and the provided elements"},
        {"intersection", (PyCFunction) ImmutableSet_intersection, METH_O,
                "Gets the intersection of this set and the provided set"},
        //{"tolist",      (PyCFunction)PVector_toList, METH_NOARGS, "Convert to list"},
        {NULL}
};

static ImmutableSetBuilder *ImmutableSetBuilder_add(ImmutableSetBuilder *self, PyObject *item) {

    if (!PySet_Contains(self->wrappedSet, item)) {
        PySet_Add(self->wrappedSet, item);
        PyList_Append(self->orderList, item);
    }
    // from examples this seems to be necessary when returning self
    Py_IncRef((PyObject *) self);
    return self;
}

static ImmutableSetBuilder *ImmutableSetBuilder_add_all(ImmutableSetBuilder *self, PyObject *args) {
    PyObject *argObj = NULL;  /* list of arguments */
    PyObject *it;
    PyObject *(*iternext)(PyObject *);

    if (!PyArg_ParseTuple(args, "|O", &argObj)) {
        return NULL;
    }

    it = PyObject_GetIter(argObj);
    if (it == NULL) {
        return NULL;
    }

    iternext = *Py_TYPE(it)->tp_iternext;
    PyObject *item = iternext(it);

    while (item != NULL) {
        ImmutableSetBuilder_add(self, item);
        item = iternext(it);
    }

    Py_INCREF(self);
    return self;
}

static PyObject *sortWithKey(PyObject *list, PyObject *key) {
    PyObject *argTuple = PyTuple_New(0);
    PyObject *keywords = PyDict_New();
    PyObject *keyString = PyUnicode_FromString("key");
    PyDict_SetItem(keywords, keyString, key);

    PyObject *sortMethod = PyObject_GetAttrString(list, "sort");

    PyObject *retVal = PyObject_Call(sortMethod, argTuple, keywords);

    Py_DECREF(keyString);
    Py_DECREF(keywords);
    Py_DECREF(argTuple);

    return retVal;
}

static ImmutableSet *ImmutableSetBuilder_build(ImmutableSetBuilder *self) {
    // currently we always require this extra copy of the fields in case
    // the builder is reused after more is added.  We can make this more
    // efficient in the future by only triggering a copy if .add() is called
    // on a builder which has already been built
    // note that if we do this we need to be careful about
    // ImmutableSetBuilder_dealloc

    // special case an empty set with a singleton
    if (PyObject_Length(self->orderList) == 0) {
        Py_INCREF(EMPY_SET);
        return EMPY_SET;
    }

    ImmutableSet *immutableset = PyObject_GC_New(ImmutableSet, &ImmutableSetType);
    PyObject_GC_Track((PyObject *) immutableset);
    immutableset->orderList = PyList_New(0);
    immutableset->wrappedSet = PySet_New(NULL);

    _PyList_Extend((PyListObject *) immutableset->orderList, self->orderList);
    _PySet_Update(immutableset->wrappedSet, self->wrappedSet);

    if (self->orderKey != NULL) {
        if (sortWithKey(immutableset->orderList, self->orderKey) == NULL) {
            return NULL;
        }
    }

    return immutableset;
}

static PyMethodDef ImmutableSetBuilder_methods[] = {
        {"add",     (PyCFunction) ImmutableSetBuilder_add,     METH_O,       "Add a value to an immutable set"},
        {"add_all", (PyCFunction) ImmutableSetBuilder_add_all, METH_VARARGS, "Adds multiple values to an immutable set"},
        {"build",   (PyCFunction) ImmutableSetBuilder_build,   METH_NOARGS,  "Build an immutable set"}
};

static PyObject *ImmutableSet_repr(ImmutableSet *self) {
    // Reuse the list repr code, a bit less efficient but saves some code
    PyObject *list_repr = PyObject_Repr(self->orderList);

    if(list_repr == NULL) {
        // Exception raised during call to repr
        return NULL;
    }

    PyObject *s = PyUnicode_FromFormat("%s%U%s", "i{", list_repr, "}");
    Py_DECREF(list_repr);

    return s;
}

// str and repr differ only in that rep has i prefix to indicate immutability
static PyObject *ImmutableSet_str(ImmutableSet *self) {
    // Reuse the list repr code, a bit less efficient but saves some code
    PyObject *list_str = PyObject_Str(self->orderList);

    if (list_str == NULL) {
        // Exception raised during call to repr
        return NULL;
    }

    PyObject *s = PyUnicode_FromFormat("%s%U%s", "{", list_str, "}");
    Py_DECREF(list_str);

    return s;
}

static int ImmutableSet_traverse(ImmutableSet *o, visitproc visit, void *arg) {
    Py_VISIT(o->orderList);
    Py_VISIT(o->wrappedSet);
}

static PyObject *ImmutableSet_richcompare(ImmutableSet *v, PyObject *w, int op) {
    return PyObject_RichCompare(v->wrappedSet, w, op);
}

static PyObject *ImmutableSet_iter(ImmutableSet *self) {
    return PyObject_GetIter(self->orderList);
}

static PyTypeObject ImmutableSetType = {
        PyVarObject_HEAD_INIT(NULL, 0)
        "immutablecollections.ImmutableSet",                         /* tp_name        */
        sizeof(ImmutableSet),                            /* tp_basicsize   */
        0,		                              /* tp_itemsize    */
(destructor)ImmutableSet_dealloc,                /* tp_dealloc     */
        0,                                          /* tp_print       */
        0,                                          /* tp_getattr     */
        0,                                          /* tp_setattr     */
        0,                                          /* tp_compare     */
        (reprfunc)ImmutableSet_repr,                     /* tp_repr        */
        0,                                          /* tp_as_number   */
        &ImmutableSet_sequence_methods,                  /* tp_as_sequence */
        0,                                         /* tp_as_mapping  */
        (hashfunc) ImmutableSet_hash,                     /* tp_hash        */
        0,                                          /* tp_call        */
        (reprfunc) ImmutableSet_str,                                          /* tp_str         */
        0,                                          /* tp_getattro    */
        0,                                          /* tp_setattro    */
        0,                                          /* tp_as_buffer   */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,    /* tp_flags       */
        "ImmutableSet",   	              /* tp_doc         */
        (traverseproc) ImmutableSet_traverse,             /* tp_traverse       */
        0,                                          /* tp_clear          */
        (richcmpfunc) ImmutableSet_richcompare,                        /* tp_richcompare    */
        // TODO: what is this?
        0/*offsetof(ImmutableSet, in_weakreflist)*/,          /* tp_weaklistoffset */
        (getiterfunc) ImmutableSet_iter,                           /* tp_iter           */
        0,                                          /* tp_iternext       */
        ImmutableSet_methods,                            /* tp_methods        */
        ImmutableSet_members,                            /* tp_members        */
        0,                                          /* tp_getset         */
        0,                                          /* tp_base           */
        0,                                          /* tp_dict           */
        0,                                          /* tp_descr_get      */
        0,                                          /* tp_descr_set      */
        0,                                          /* tp_dictoffset     */
};

static PyTypeObject ImmutableSetBuilderType = {
        PyVarObject_HEAD_INIT(NULL, 0)
        "immutablecollections.ImmutableSetBuilder",                         /* tp_name        */
        sizeof(ImmutableSetBuilder),                            /* tp_basicsize   */
        0,                                      /* tp_itemsize    */
        (destructor) ImmutableSetBuilder_dealloc,                /* tp_dealloc     */
        0,                                          /* tp_print       */
        0,                                          /* tp_getattr     */
        0,                                          /* tp_setattr     */
        0,                                          /* tp_compare     */
        0,                     /* tp_repr        */
        0,                                          /* tp_as_number   */
        0,                  /* tp_as_sequence */
        0,                                         /* tp_as_mapping  */
        0,                     /* tp_hash        */
        0,                                          /* tp_call        */
        0,                                          /* tp_str         */
        0,                                          /* tp_getattro    */
        0,                                          /* tp_setattro    */
        0,                                          /* tp_as_buffer   */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,    /* tp_flags       */
        "ImmutableSetBuilder",                  /* tp_doc         */
        // TODO: test traverse
        0,             /* tp_traverse       */
        0,                                          /* tp_clear          */
        0,                        /* tp_richcompare    */
        // TODO: what is this?
        0/*offsetof(ImmutableSet, in_weakreflist)*/,          /* tp_weaklistoffset */
        0,                           /* tp_iter           */
        0,                                          /* tp_iternext       */
        ImmutableSetBuilder_methods,                            /* tp_methods        */
        ImmutableSetBuilder_members,                            /* tp_members        */
        0,                                          /* tp_getset         */
        0,                                          /* tp_base           */
        0,                                          /* tp_dict           */
        0,                                          /* tp_descr_get      */
        0,                                          /* tp_descr_set      */
        0,                                          /* tp_dictoffset     */
};

static ImmutableSet *immutablecollections_immutableset(PyObject *self, PyObject *args) {
    PyObject *argObj = NULL;  /* list of arguments */
    PyObject *it;
    PyObject *(*iternext)(PyObject *);

    if(!PyArg_ParseTuple(args, "|O", &argObj)) {
        return NULL;
    }

    it = PyObject_GetIter(argObj);
    if (it == NULL) {
        return NULL;
    }

    iternext = *Py_TYPE(it)->tp_iternext;
    PyObject *item = iternext(it);
    if (item == NULL) {
        Py_DECREF(it);
        HANDLE_ITERATION_ERROR();

        // special case an empty set with a singleton
        Py_INCREF(EMPY_SET);
        return EMPY_SET;
    } else {
        ImmutableSet *immutableset = PyObject_GC_New(ImmutableSet, &ImmutableSetType);
        immutableset->orderList = PyList_New(0);
        immutableset->wrappedSet = PySet_New(NULL);
        PyObject_GC_Track((PyObject *) immutableset);

        while (item != NULL) {
            if (!PySet_Contains(immutableset->wrappedSet, item)) {
                PySet_Add(immutableset->wrappedSet, item);
                PyList_Append(immutableset->orderList, item);
            }
            item = iternext(it);
        }

        Py_DECREF(it);
        HANDLE_ITERATION_ERROR();
        return immutableset;
    }
}

static ImmutableSetBuilder *immutablecollections_immutablesetbuilder_internal() {
    PyObject *argsTuple = PyTuple_New(0);
    PyObject *kwArgs = PyDict_New();

    // TODO: do I need to decrement the refcount for these or not? Do they automatically
    // get a count of 1 on creation?
    return immutablecollections_immutablesetbuilder(NULL, argsTuple, kwArgs);
}

static ImmutableSetBuilder *immutablecollections_immutablesetbuilder(PyObject *self,
                                                                     PyObject *args, PyObject *keywords) {
    ImmutableSetBuilder *immutablesetbuilder = PyObject_GC_New(ImmutableSetBuilder, &ImmutableSetBuilderType);
    immutablesetbuilder->orderList = PyList_New(0);
    immutablesetbuilder->wrappedSet = PySet_New(NULL);

    PyObject *key = NULL;  /* list of arguments */

    static char *kwlist[] = {"order_key", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords, "|O", kwlist, &key)) {
        return NULL;
    }

    if (key != NULL) {
        if (!PyFunction_Check(key)) {
            // TODO: presumably this should clean up the memory allocated above, ugh. Leaving this TODO since
            // in most cases this is not a recoverable error anyway
            return NULL;
        }
        Py_INCREF(key);
    }

    immutablesetbuilder->orderKey = key;

    PyObject_GC_Track((PyObject *) immutablesetbuilder);

    return immutablesetbuilder;
}


static PyMethodDef ImmutableCollectionMethods[] = {
        {"immutableset",        (PyCFunction) immutablecollections_immutableset,        METH_VARARGS,
                        "immutableset([iterable])\n"
                        "Create a new immutableset containing the elements in iterable.\n\n"
                        ">>> set1 = immutableset([1, 2, 3])\n"
                        ">>> set\n"
                        "immutableset([1, 2, 3])"},
        {"immutablesetbuilder", (PyCFunction) immutablecollections_immutablesetbuilder, METH_KEYWORDS | METH_VARARGS,
         "immutablesetbuilder()\n"
         "Create a builder for an immutableset.\n\n"
         ">>> set1_builder = immutablesetbuilder()\n"
         "set1_builder.add(1)\n"
         "set1_builder.add(2)\n"
         ">>> set1_builder.build()\n"
         "immutableset([1, 2])"},
        {NULL, NULL, 0, NULL}
};

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "immutablecollections",          /* m_name */
    "Immutable Collections", /* m_doc */
    -1,                  /* m_size */
    ImmutableCollectionMethods,   /* m_methods */
    NULL,                /* m_reload */
    NULL,                /* m_traverse */
    NULL,                /* m_clear */
    NULL,                /* m_free */
  };


PyObject* moduleinit(void) {
    PyObject* m;

    // Only allow creation/initialization through factory method pvec
    //PVectorType.tp_init = NULL;
    //PVectorType.tp_new = NULL;

    if (PyType_Ready(&ImmutableSetType) < 0) {
        return NULL;
    }

    if (PyType_Ready(&ImmutableSetBuilderType) < 0) {
        return NULL;
    }


    m = PyModule_Create(&moduledef);

    if (m == NULL) {
        return NULL;
    }

    EMPY_SET = makeEmptySet();

    Py_INCREF(&ImmutableSetType);
    PyModule_AddObject(m, "ImmutableSet", (PyObject *)&ImmutableSetType);
    Py_INCREF(&ImmutableSetBuilderType);
    PyModule_AddObject(m, "ImmutableSetBuilder", (PyObject *) &ImmutableSetBuilderType);

    return m;
}

PyMODINIT_FUNC PyInit_immutablecollections(void) {
    return moduleinit();
}


// TODO: extend abstractset
// TODO: should we also extend sequence and dispense with as_list
// from the Python version? Or do sequences have some equality guarantee?
// TODO: union - is throwing SystemError: new style getargs format but argument is not a tuple
// TODO: intersection - draft implementation but needs debugging - GC object already tracked
// TODO: difference
// TODO: type checking
// TODO: require_ordered_input
// TODO: support order_key on builder
// TODO: strip []s from repr/str
// TODO: builder methods don't appear to be visible prior to doing a dir() on the object !?!?