#ifndef FREESTYLE_PYTHON_ADJACENCYITERATOR_H
#define FREESTYLE_PYTHON_ADJACENCYITERATOR_H

#include "../../stroke/ChainingIterators.h"
#include "../BPy_Iterator.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject AdjacencyIterator_Type;

#define BPy_AdjacencyIterator_Check(v)	(( (PyObject *) v)->ob_type == &AdjacencyIterator_Type)

/*---------------------------Python BPy_AdjacencyIterator structure definition----------*/
typedef struct {
	BPy_Iterator py_it;
	AdjacencyIterator *ai;
} BPy_AdjacencyIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_ADJACENCYITERATOR_H */
