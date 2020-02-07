#include "Inference.h"
#include "Term.h"

#define DERIVATION_STAMP(a,b) Stamp conclusionStamp = Stamp_make(&a->stamp, &b->stamp); \
                              long creationTime = MAX(a->creationTime, b->creationTime);
#define DERIVATION_STAMP_AND_TIME(a,b) DERIVATION_STAMP(a,b) \
                long conclusionTime = b->occurrenceTime; \
                Truth truthA = Truth_Projection(a->truth, a->occurrenceTime, conclusionTime); \
                Truth truthB = b->truth;
                
static double weighted_average(double a1, double a2, double w1, double w2)
{
    return (a1*w1+a2*w2)/(w1+w2);
}
                
//{Event a., Event b.} |- Event (&/,a,b).
Event Inference_BeliefIntersection(Event *a, Event *b)
{
    assert(b->occurrenceTime >= a->occurrenceTime, "after(b,a) violated in Inference_BeliefIntersection");
    DERIVATION_STAMP_AND_TIME(a,b)
    return (Event) { .term = Narsese_Sequence(&a->term, &b->term),
                     .type = EVENT_TYPE_BELIEF,
                     .truth = Truth_Intersection(truthA, truthB),
                     .stamp = conclusionStamp, 
                     .occurrenceTime = conclusionTime,
                     .creationTime = creationTime };
}

//{Event a., Event b., after(b,a)} |- Implication <a =/> b>.
Implication Inference_BeliefInduction(Event *a, Event *b)
{
    assert(b->occurrenceTime > a->occurrenceTime, "after(b,a) violated in Inference_BeliefInduction");
    DERIVATION_STAMP_AND_TIME(a,b)
    Term term = {0};
    term.atoms[0] = Narsese_AtomicTermIndex("$");
    Term_OverrideSubterm(&term, 1, &a->term);
    Term_OverrideSubterm(&term, 2, &b->term);
    return (Implication) { .term = Term_WithHash(term), 
                           .truth = Truth_Eternalize(Truth_Induction(truthA, truthB)),
                           .stamp = conclusionStamp,
                           .occurrenceTimeOffset = b->occurrenceTime - a->occurrenceTime,
                           .creationTime = creationTime };
}

//{Event a., Event a.} |- Event a.
//{Event a!, Event a!} |- Event a!
static Event Inference_EventRevision(Event *a, Event *b)
{
    DERIVATION_STAMP_AND_TIME(a,b)
    return (Event) { .term = a->term,
                     .type = a->type,
                     .truth = Truth_Revision(truthA, truthB),
                     .stamp = conclusionStamp, 
                     .occurrenceTime = conclusionTime,
                     .creationTime = creationTime };
}

//{Implication <a =/> b>., <a =/> b>.} |- Implication <a =/> b>.
Implication Inference_ImplicationRevision(Implication *a, Implication *b)
{
    DERIVATION_STAMP(a,b)
    double occurrenceTimeOffsetAvg = weighted_average(a->occurrenceTimeOffset, b->occurrenceTimeOffset, Truth_c2w(a->truth.confidence), Truth_c2w(b->truth.confidence));
    return (Implication) { .term = a->term,
                           .truth = Truth_Revision(a->truth, b->truth),
                           .stamp = conclusionStamp, 
                           .occurrenceTimeOffset = occurrenceTimeOffsetAvg,
                           .sourceConcept = a->sourceConcept,
                           .sourceConceptTerm = a->sourceConceptTerm,
                           .creationTime = creationTime };
}

//{Event b!, Implication <a =/> b>.} |- Event a!
Event Inference_GoalDeduction(Event *component, Implication *compound)
{
    assert(Narsese_copulaEquals(compound->term.atoms[0],'$'), "Not a valid implication term!");
    DERIVATION_STAMP(component,compound)
    Term precondition = Term_ExtractSubterm(&compound->term, 1);
    //extract precondition: (plus unification once vars are there)
    return (Event) { .term = Narsese_GetPreconditionWithoutOp(&precondition), 
                     .type = EVENT_TYPE_GOAL, 
                     .truth = Truth_Deduction(compound->truth, component->truth),
                     .stamp = conclusionStamp, 
                     .occurrenceTime = component->occurrenceTime - compound->occurrenceTimeOffset,
                     .creationTime = creationTime };
}

//{Event a.} |- Event a. updated to currentTime
Event Inference_EventUpdate(Event *ev, long currentTime)
{
    Event ret = *ev;
    ret.truth = Truth_Projection(ret.truth, ret.occurrenceTime, currentTime);
    ret.occurrenceTime = currentTime;
    return ret;
}

//{Event (&/,a,op())!, Event a.} |- Event op()!
Event Inference_OperationDeduction(Event *compound, Event *component, long currentTime)
{
    DERIVATION_STAMP(component,compound)
    Event compoundUpdated = Inference_EventUpdate(compound, currentTime);
    Event componentUpdated = Inference_EventUpdate(component, currentTime);
    return (Event) { .term = compound->term,
                     .type = EVENT_TYPE_GOAL, 
                     .truth = Truth_Deduction(compoundUpdated.truth, componentUpdated.truth),
                     .stamp = conclusionStamp, 
                     .occurrenceTime = compound->occurrenceTime,
                     .creationTime = creationTime };
}

//{Event a!, Event a!} |- Event a! (revision and choice)
Event Inference_IncreasedActionPotential(Event *existing_potential, Event *incoming_spike, long currentTime, bool *revised)
{
    if(revised != NULL)
    {
        *revised = false;
    }
    if(existing_potential->type == EVENT_TYPE_DELETED)
    {
        return *incoming_spike;
    }
    else
    {
        double confExisting = Inference_EventUpdate(existing_potential, currentTime).truth.confidence;
        double confIncoming = Inference_EventUpdate(incoming_spike, currentTime).truth.confidence;
        //check if there is evidental overlap
        bool overlap = Stamp_checkOverlap(&incoming_spike->stamp, &existing_potential->stamp);
        //if there is or the terms aren't equal, apply choice, keeping the stronger one:
        if(overlap || !Term_Equal(&existing_potential->term, &incoming_spike->term))
        {
            if(confIncoming > confExisting)
            {
                return *incoming_spike;
            }
        }
        else
        //and else revise, increasing the "activation potential"
        {
            Event revised_spike = Inference_EventRevision(existing_potential, incoming_spike);
            if(revised_spike.truth.confidence >= existing_potential->truth.confidence)
            {
                if(revised != NULL)
                {
                    *revised = true;
                }
                return revised_spike;
            }
            //lower, also use choice
            if(confIncoming > confExisting)
            {
                return *incoming_spike;
            }
        }
    }
    return *existing_potential;
}

//{Event a., Implication <a =/> b>.} |- Event b.
Event Inference_BeliefDeduction(Event *component, Implication *compound)
{
    assert(Narsese_copulaEquals(compound->term.atoms[0],'$'), "Not a valid implication term!");
    DERIVATION_STAMP(component,compound)
    Term postcondition = Term_ExtractSubterm(&compound->term, 2);
    return (Event) { .term = Term_WithHash(postcondition), 
                     .type = EVENT_TYPE_BELIEF, 
                     .truth = Truth_Deduction(compound->truth, component->truth),
                     .stamp = conclusionStamp, 
                     .occurrenceTime = component->occurrenceTime == OCCURRENCE_ETERNAL ? 
                                                                    OCCURRENCE_ETERNAL : component->occurrenceTime + compound->occurrenceTimeOffset,
                     .creationTime = creationTime };
}
