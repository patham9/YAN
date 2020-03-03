/* 
 * The MIT License
 *
 * Copyright 2020 The OpenNARS authors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "Truth.h"

double TRUTH_EVIDENTAL_HORIZON = TRUTH_EVIDENTAL_HORIZON_INITIAL;
double TRUTH_PROJECTION_DECAY = TRUTH_PROJECTION_DECAY_INITIAL;
#define TruthValues(v1,v2, f1,c1, f2,c2) double f1 = v1.frequency; double f2 = v2.frequency; double c1 = v1.confidence; double c2 = v2.confidence;

double Truth_w2c(double w)
{
    return w / (w + TRUTH_EVIDENTAL_HORIZON);
}

double Truth_c2w(double c)
{
    return TRUTH_EVIDENTAL_HORIZON * c / (1 - c);
}

double Truth_Expectation(Truth v)
{
    return (v.confidence * (v.frequency - 0.5) + 0.5);
}

Truth Truth_Revision(Truth v1, Truth v2)
{
    TruthValues(v1,v2, f1,c1, f2,c2);
    double w1 = Truth_c2w(c1);
    double w2 = Truth_c2w(c2);
    double w = w1 + w2;
    return (Truth) { .frequency = MIN(1.0, (w1 * f1 + w2 * f2) / w), 
                     .confidence = MIN(MAX_CONFIDENCE, MAX(MAX(Truth_w2c(w), c1), c2)) };
}

Truth Truth_Deduction(Truth v1, Truth v2)
{
    TruthValues(v1,v2, f1,c1, f2,c2);
    double f = f1 * f2;
    return (Truth) { .frequency = f, .confidence = c1 * c2 * f };
}

Truth Truth_Abduction(Truth v1, Truth v2)
{
    TruthValues(v1,v2, f1,c1, f2,c2);
    return (Truth) { .frequency = f1, .confidence = Truth_w2c(f2 * c1 * c2) };
}

Truth Truth_Induction(Truth v1, Truth v2)
{
    return Truth_Abduction(v2, v1);
}

Truth Truth_Intersection(Truth v1, Truth v2)
{
    TruthValues(v1,v2, f1,c1, f2,c2);
    return (Truth) { .frequency = f1 * f2, .confidence = c1 * c2 };
}

Truth Truth_Eternalize(Truth v)
{
    return (Truth) { .frequency = v.frequency, .confidence = Truth_w2c(v.confidence) };
}

Truth Truth_Projection(Truth v, long originalTime, long targetTime)
{
    double difference = labs(targetTime - originalTime);
    return originalTime == OCCURRENCE_ETERNAL ? 
           v : (Truth) { .frequency = v.frequency, .confidence = v.confidence * pow(TRUTH_PROJECTION_DECAY,difference) };
}

void Truth_Print(Truth *truth)
{
    printf("Truth: frequency=%f, confidence=%f\n", truth->frequency, truth->confidence);
}

//not part of MSC:

Truth Truth_Exemplification(Truth v1, Truth v2)
{
    TruthValues(v1,v2, f1,c1, f2,c2);
    return (Truth) { .frequency = 1.0, .confidence = Truth_w2c(f1 * f2 * c1 * c2) };
}

static inline double or(double a, double b)
{
    return 1.0 - (1.0 - a) * (1.0 - b);
}

Truth Truth_Comparison(Truth v1, Truth v2)
{
    TruthValues(v1,v2, f1,c1, f2,c2);
    double f0 = or(f1, f2);
    return (Truth) { .frequency = (f0 == 0.0) ? 0.0 : ((f1*f2) / f0), .confidence = Truth_w2c(f0 * c1 * c2) };
}

Truth Truth_Analogy(Truth v1, Truth v2)
{
    TruthValues(v1,v2, f1,c1, f2,c2);
    return (Truth) { .frequency = f1 * f2, .confidence = c1 * c2 * f2 };
}

Truth Truth_Resemblance(Truth v1, Truth v2)
{
    TruthValues(v1,v2, f1,c1, f2,c2);
    return (Truth) { .frequency = f1 * f2, .confidence = c1 * c2 * or(f1, f2) };
}

Truth Truth_Union(Truth v1, Truth v2)
{
    TruthValues(v1,v2, f1,c1, f2,c2);
    return (Truth) { .frequency = or(f1, f2), .confidence = c1 * c2 };
}

Truth Truth_Difference(Truth v1, Truth v2)
{
    TruthValues(v1,v2, f1,c1, f2,c2);
    return (Truth) { .frequency = f1 * (1.0 - f2), .confidence = c1 * c2 };
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

Truth Truth_Negation(Truth v1, Truth v2)
{
    TruthValues(v1,v2, f1,c1, f2,c2);
    return (Truth) { .frequency = 1.0-f1, .confidence = c1 };
}

Truth Truth_Conversion(Truth v1, Truth v2)
{
    return (Truth) { .frequency = 1.0, .confidence = Truth_w2c(v1.frequency * v1.confidence) };
}

Truth Truth_StructuralDeduction(Truth v1, Truth v2)
{
    return Truth_Deduction(v1, STRUCTURAL_TRUTH);
}

Truth Truth_StructuralAbduction(Truth v1, Truth v2)
{
    return Truth_Abduction(v1, STRUCTURAL_TRUTH);
}

#pragma GCC diagnostic pop

bool Truth_Equal(Truth *v1, Truth *v2)
{
    return v1->confidence == v2->confidence && v1->frequency == v2->frequency;
}
