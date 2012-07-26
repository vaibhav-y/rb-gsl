/////////////////////////////////////////////////////////////////////
// = NMatrix
//
// A linear algebra library for scientific computation in Ruby.
// NMatrix is part of SciRuby.
//
// NMatrix was originally inspired by and derived from NArray, by
// Masahiro Tanaka: http://narray.rubyforge.org
//
// == Copyright Information
//
// SciRuby is Copyright (c) 2010 - 2012, Ruby Science Foundation
// NMatrix is Copyright (c) 2012, Ruby Science Foundation
//
// Please see LICENSE.txt for additional copyright notices.
//
// == Contributing
//
// By contributing source code to SciRuby, you agree to be bound by
// our Contributor Agreement:
//
// * https://github.com/SciRuby/sciruby/wiki/Contributor-Agreement
//
// == dense.c
//
// Dense n-dimensional matrix storage.

/*
 * Standard Includes
 */

#include <ruby.h>

/*
 * Project Includes
 */

#include "types.h"

#include "data/data.h"

#include "common.h"
#include "dense.h"

/*
 * Macros
 */

/*
 * Global Variables
 */

/*
 * Forward Declarations
 */

template <typename DType, typename NewDType>
DENSE_STORAGE* dense_storage_cast_copy_template(const DENSE_STORAGE* rhs, dtype_t new_dtype);

template <typename LDType, typename RDType>
bool dense_storage_eqeq_template(const DENSE_STORAGE* left, const DENSE_STORAGE* right);

template <typename DType>
bool dense_storage_is_hermitian_template(const DENSE_STORAGE* mat, int lda);

template <typename DType>
bool dense_storage_is_symmetric_template(const DENSE_STORAGE* mat, int lda);

/*
 * Functions
 */

///////////////
// Lifecycle //
///////////////

/*
 * Note that elements and elements_length are for initial value(s) passed in.
 * If they are the correct length, they will be used directly. If not, they
 * will be concatenated over and over again into a new elements array. If
 * elements is NULL, the new elements array will not be initialized.
 */
DENSE_STORAGE* dense_storage_create(dtype_t dtype, size_t* shape, size_t rank, void* elements, size_t elements_length) {
  DENSE_STORAGE* s;
  size_t count, i, copy_length = elements_length;

  s = ALLOC( DENSE_STORAGE );

  s->rank       = rank;
  s->shape      = shape;
  s->dtype      = dtype;
  s->offset     = (size_t*) calloc(sizeof(size_t), rank);
  s->count      = 1;
  s->src        = s;
	
	count         = storage_count_max_elements(s->rank, s->shape);

  if (elements_length == count) {
  	s->elements = elements;
  	
  } else {
    s->elements = ALLOC_N(char, DTYPE_SIZES[dtype]*count);

    if (elements_length > 0) {
      // Repeat elements over and over again until the end of the matrix.
      for (i = 0; i < count; i += elements_length) {
        
        if (i + elements_length > count) {
        	copy_length = count - i;
        }
        
        memcpy((char*)(s->elements)+i*DTYPE_SIZES[dtype], (char*)(elements)+(i % elements_length)*DTYPE_SIZES[dtype], copy_length*DTYPE_SIZES[dtype]);
      }

      // Get rid of the init_val.
      free(elements);
    }
  }

  return s;
}

/*
 * Documentation goes here.
 */
void dense_storage_delete(DENSE_STORAGE* s) {
  // Sometimes Ruby passes in NULL storage for some reason (probably on copy construction failure).
  if (s) {
    if(s->count <= 1) {
      free(s->shape);
      free(s->offset);
      free(s->elements);
      free(s);
    }
  }
}

/*
 * Documentation goes here.
 */
void dense_storage_delete_ref(DENSE_STORAGE* s) {
  // Sometimes Ruby passes in NULL storage for some reason (probably on copy construction failure).
  if (s) {
    ((DENSE_STORAGE*)s->src)->count--;
    free(s->shape);
    free(s->offset);
    free(s);
  }
}

/*
 * Documentation goes here.
 */
void dense_storage_mark(DENSE_STORAGE* storage) {
  size_t index;
	
	VALUE* els = (VALUE*)storage->elements;
	
  if (storage && storage->dtype == RUBYOBJ) {
  	for (index = storage_count_max_elements(storage->rank, storage->shape); index-- > 0;) {
      rb_gc_mark(els[index]);
    }
  }
}

///////////////
// Accessors //
///////////////

/*
 * Documentation goes here.
 */
void* dense_storage_get(DENSE_STORAGE* s, SLICE* slice) {
  DENSE_STORAGE *ns;

  if (slice->is_one_el) {
    return (char*)(s->elements) + dense_storage_pos(s, slice) * DTYPE_SIZES[s->dtype];
    
  } else {
    ns = ALLOC( DENSE_STORAGE );

    ns->rank       = s->rank;
    ns->shape      = slice->lens;
    ns->dtype      = s->dtype;
    ns->offset     = slice->coords;
    ns->elements   = s->elements;
    
    s->count++;
    ns->src = (void*)s;

    return ns;
  }
}


/*
 * Does not free passed-in value! Different from list_storage_insert.
 */
void dense_storage_set(DENSE_STORAGE* s, SLICE* slice, void* val) {
  memcpy((char*)(s->elements) + dense_storage_pos(s, slice) * DTYPE_SIZES[s->dtype], val, DTYPE_SIZES[s->dtype]);
}

///////////
// Tests //
///////////

/*
 * Do these two dense matrices have the same contents?
 *
 * TODO: Test the shape of the two matrices.
 * TODO: See if using memcmp is faster when the left- and right-hand matrices
 *				have the same dtype.
 */
bool dense_storage_eqeq(const DENSE_STORAGE* left, const DENSE_STORAGE* right) {
	LR_DTYPE_TEMPLATE_TABLE(dense_storage_eqeq_template, bool, const DENSE_STORAGE*, const DENSE_STORAGE*);
	
	return ttable[left->dtype][right->dtype](left, right);
}

/*
 * Test to see if the matrix is Hermitian.  If the matrix does not have a
 * dtype of Complex64 or Complex128 this is the same as testing for symmetry.
 */
bool dense_storage_is_hermitian(const DENSE_STORAGE* mat, int lda) {
	if (mat->dtype == COMPLEX64) {
		return dense_storage_is_hermitian_template<Complex64>(mat, lda);
		
	} else if (mat->dtype == COMPLEX128) {
		return dense_storage_is_hermitian_template<Complex128>(mat, lda);
		
	} else {
		return dense_storage_is_symmetric(mat, lda);
	}
}

/*
 * Is this dense matrix symmetric about the diagonal?
 */
bool dense_storage_is_symmetric(const DENSE_STORAGE* mat, int lda) {
	DTYPE_TEMPLATE_TABLE(dense_storage_is_symmetric_template, bool, const DENSE_STORAGE*, int);
	
	return ttable[mat->dtype](mat, lda);
}

/////////////
// Utility //
/////////////

/*
 * Documentation goes here.
 */
size_t dense_storage_pos(DENSE_STORAGE* s, SLICE* slice) {
  size_t k, l;
  size_t inner, outer = 0;
  
  for (k = s->rank; k-- > 0;) {
  	inner = slice->coords[k] + s->offset[k];
    
    for (l = k+1; l < s->rank; ++l) {
      inner *= ((DENSE_STORAGE*)s->src)->shape[l];
    }
    
    outer += inner;
  }
  
  return outer;
}

/////////////////////////
// Copying and Casting //
/////////////////////////

/*
 * Documentation goes here.
 */
DENSE_STORAGE* dense_storage_cast_copy(const DENSE_STORAGE* rhs, dtype_t new_dtype) {
	LR_DTYPE_TEMPLATE_TABLE(dense_storage_cast_copy_template, DENSE_STORAGE*, const DENSE_STORAGE*, dtype_t);
	
	return ttable[new_dtype][rhs->dtype](rhs, new_dtype);
}

/*
 * Documentation goes here.
 */
DENSE_STORAGE* dense_storage_copy(DENSE_STORAGE* rhs) {
  DENSE_STORAGE* lhs;
  
  size_t  count = storage_count_max_elements(rhs->rank, rhs->shape), p;
  size_t* shape = ALLOC_N(size_t, rhs->rank);
  
  if (!shape) {
  	return NULL;
  }

  // copy shape array
  for (p = rhs->rank; p-- > 0;) {
    shape[p] = rhs->shape[p];
  }

  lhs = dense_storage_create(rhs->dtype, shape, rhs->rank, NULL, 0);

	// Ensure that allocation worked before copying.
  if (lhs && count) {
    memcpy(lhs->elements, rhs->elements, DTYPE_SIZES[rhs->dtype] * count);
  }

  return lhs;
}

/////////////////////////
// Templated Functions //
/////////////////////////

template <typename DType, typename NewDType>
DENSE_STORAGE* dense_storage_cast_copy_template(const DENSE_STORAGE* rhs, dtype_t new_dtype) {
  size_t  count = storage_count_max_elements(rhs->rank, rhs->shape), p;
  size_t* shape = ALLOC_N(size_t, rhs->rank);
  
  DType*		rhs_els = (DType*)rhs->elements;
  NewDType* lhs_els;
  
  DENSE_STORAGE* lhs;
  
  if (!shape) {
  	return NULL;
  }
	
  // Copy shape array.
  for (p = rhs->rank; p-- > 0;) {
  	shape[p] = rhs->shape[p];
  }
	
  lhs			= dense_storage_create(new_dtype, shape, rhs->rank, NULL, 0);
  lhs_els	= (NewDType*)lhs->elements;

	// Ensure that allocation worked before copying.
  if (lhs && count) {
    if (lhs->dtype == rhs->dtype) {
      memcpy(lhs->elements, rhs->elements, DTYPE_SIZES[rhs->dtype] * count);
      
    } else {
    	while (count-- > 0) {
    		lhs_els[count] = rhs_els[count];
      }
    }
  }
	
  return lhs;
}

template <typename LDType, typename RDType>
bool dense_storage_eqeq_template(const DENSE_STORAGE* left, const DENSE_STORAGE* right) {
	int index;
	
	LDType* left_els	= (LDType*)left->elements;
	RDType* right_els	= (RDType*)right->elements;
	
	for (index = storage_count_max_elements(left->rank, left->shape); index-- > 0;) {
		if (left_els[index] != right_els[index]) {
			return false;
		}
	}
	
	return true;
}

template <typename DType>
bool dense_storage_is_hermitian_template(const DENSE_STORAGE* mat, int lda) {
	unsigned int i, j;
	register DType complex_conj;
	
	const DType* els = (DType*) mat->elements;
	
	for (i = mat->shape[0]; i-- > 0;) {
		for (j = i + 1; j < mat->shape[1]; ++j) {
			complex_conj		= els[j*lda + 1];
			complex_conj.i	= -complex_conj.i;
			
			if (els[i*lda+j] != complex_conj) {
	      return false;
	    }
		}
	}
	
	return true;
}

template <typename DType>
bool dense_storage_is_symmetric_template(const DENSE_STORAGE* mat, int lda) {
	unsigned int i, j;
	const DType* els = (DType*) mat->elements;
	
	for (i = mat->shape[0]; i-- > 0;) {
		for (j = i + 1; j < mat->shape[1]; ++j) {
			if (els[i*lda+j] != els[j*lda+i]) {
	      return false;
	    }
		}
	}
	
	return true;
}
