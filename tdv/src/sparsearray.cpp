#include <cmath>
#include "sparsearray.h"

float SparseArray::cosine(const SparseArray& a, const SparseArray& b)
{
    float cos =  ((a * b) / (a.norm() * b.norm()));

    // Clipping underflow to zero.
    if (!std::isnan(cos))
        return cos;
    else
        return 0.0;
}

float SparseArray::weightedCosine(const SparseArray& a, const SparseArray& b, float positiveWeight, float negativeWeight)
{
    float norm = a.norm() * b.norm();
    float dot = 0;

    for (auto it = a.begin(); it != a.end(); ++it)
    {
        if (b.count(it->first))
        {
            float prod = it->second * b.at(it->first);
            if (prod >= 0)
                prod *= positiveWeight;
            else
                prod *= negativeWeight;

            dot += prod;
        }
    }

    float cos = dot / norm;

    // Clipping underflow to zero.
    if (!std::isnan(cos))
        return cos;
    else
        return 0.0;
}

// Euclidean norm.
float SparseArray::norm() const
{
    float sqrSum = 0;

    for (auto it = this->begin(); it != this->end(); ++it)
    {
        sqrSum += it->second * it->second;
    }

    return sqrt(sqrSum);
}

void SparseArray::toCArray(float *v, ulong size) const
{
    for (ulong i = 0; i < size; i++)
        v[i] = 0;

    for (auto it = this->begin(); it != this->end(); ++it)
    {
        if (it->first < size)
            v[it->first] = it->second;
    }
}

SparseArray SparseArray::fromCArray(const float *v, ulong size, float thresholdMin)
{
    SparseArray sv;

    for (ulong i = 0; i < size; i++)
    {
        if (v[i] >= thresholdMin)
            sv[i] = v[i];
    }

    return sv;
}

float SparseArray::get(const ulong& k) const
{
    if (count(k))
        return this->at(k);
    else
        return 0.0;
} 

// Sparse array sum.
SparseArray& SparseArray::operator+= (const SparseArray& other)
{
    for (auto it = other.begin(); it != other.end(); ++it)
    {
        if (this->count(it->first))
        {
            (*this)[it->first] += it->second;
        }
        else
        {
            (*this)[it->first] = it->second;
        }
    }

    return *this;
}

SparseArray SparseArray::operator+ (const SparseArray& other) const
{
    return SparseArray(*this) += other;
}

// Dot product.
float SparseArray::operator* (const SparseArray& other) const
{
    float dot = 0;

    for (auto it = this->begin(); it != this->end(); ++it)
    {
        if (other.count(it->first))
        {
            dot += it->second * other.at(it->first);
        }
    }

    return dot;
}

SparseArray& SparseArray::operator/= (float div)
{
    for (auto it = this->begin(); it != this->end(); ++it)
    {
        (*this)[it->first] /= div;
    }

    return *this;
}


SparseArray SparseArray::operator/ (float div) const
{
    return SparseArray(*this) /= div;
}

