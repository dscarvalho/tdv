#ifndef SPARSEARRAY_H
#define SPARSEARRAY_H
#include <map>
#include "types.h"

class SparseArray : public std::map<ulong, float>
{
    typedef umap<ulong, float> super;

    public:
    static float cosine(const SparseArray& a, const SparseArray& b); 
    static float weightedCosine(const SparseArray& a, const SparseArray& b, float positiveWeight, float negativeWeight);

    // Euclidean norm.
    float norm() const;

    // Size of key intersection.
    static size_t keyIntersectionSize(const SparseArray& a, const SparseArray& b);
    
    //Conversion to plain C array.
    void toCArray(float *v, ulong size) const;
    static SparseArray fromCArray(const float *v, ulong size, float thresholdMin);
    
    float get(const ulong& k) const;

    // Sparse array sum.
    SparseArray operator+ (const SparseArray& other) const; 
    SparseArray& operator+= (const SparseArray& other);

    // Dot product.
    float operator* (const SparseArray& other) const; 
    
    SparseArray& operator/= (float div);
    SparseArray operator/ (float div) const;
};

#endif
