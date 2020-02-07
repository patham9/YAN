#include "Term.h"

void Term_Print(Term *term)
{
    for(int i=0; i<MAX_SEQUENCE_LEN; i++)
    {
        if(term->atoms[i] != 0)
        {
            printf("%d", (int) term->atoms[i]);
        }
        else
        {
            fputs("@", stdout);
        }
    }
}

bool Term_Equal(Term *a, Term *b) //either both have hashes (like in RuleTabel where terems are shortlived)
{   //or none for the comparison to succeed, and in debug mode it's made sure they don't differ when the terms are equal (expensive!)
    if(a->hash != b->hash)
    {
        return false;
    }
    IN_DEBUG
    ( 
        assert(a->hash > 0 && b->hash > 0, "Unhashed terms are compared!");
        bool equal = memcmp(&a->atoms, &b->atoms, COMPOUND_TERM_SIZE_MAX*sizeof(Atom)) == 0; 
        assert(!(equal && a->hash != b->hash), "Equal terms although hashes differ");
    )
    return memcmp(a, b, sizeof(Term)) == 0;
}

static bool Term_RelativeOverride(Term *term, int i, Term *subterm, int j)
{
    if(i >= COMPOUND_TERM_SIZE_MAX)
    {
        return false;
    }
    if(j < COMPOUND_TERM_SIZE_MAX)
    {
        term->atoms[i] = subterm->atoms[j];
        int left_in_subterm = (j+1)*2-1;
        if(left_in_subterm < COMPOUND_TERM_SIZE_MAX && subterm->atoms[left_in_subterm] != 0)
        {
            if(!Term_RelativeOverride(term, (i+1)*2-1, subterm, left_in_subterm))   //override left child
            {
                return false;
            }
        }
        int right_in_subterm = (j+1)*2+1-1;
        if(right_in_subterm < COMPOUND_TERM_SIZE_MAX && subterm->atoms[right_in_subterm] != 0)
        {
            if(!Term_RelativeOverride(term, (i+1)*2+1-1, subterm, right_in_subterm)) //override right child
            {
                return false;
            }
        }
    }
    return true;
}

bool Term_OverrideSubterm(Term *term, int i, Term *subterm)
{
    return Term_RelativeOverride(term, i, subterm, 0); //subterm starts at its root, but its a subterm in term at position i
}

Term Term_ExtractSubterm(Term *term, int j)
{
    Term ret = {0}; //ret is where to "write into" 
    Term_RelativeOverride(&ret, 0, term, j); //where we begin to write at root, 0 (always succeeds as we extract just a subset) reading from term beginning at i
    return ret; //Term_WithHash(ret); //DEPENDS IF RULE TABLE SHOULD USE HASHES, MIGHT BE SLOW SINCE THEY WOULD BE OFTEN CALCULATED!
}

int Term_Complexity(Term *term)
{
    int s = 0;
    for(int i=0; i<COMPOUND_TERM_SIZE_MAX; i++)
    {
        if(term->atoms[i])
        {
            s += 1;
        }
    }
    return s;
}

Term Term_WithHash(Term term)
{
    if(term.hash != 0)
    {
        return term; //already hashed!
    }
    int pieces = TERM_ATOMS_SIZE / TERM_HASH_TYPE_SIZE;
    assert(TERM_HASH_TYPE_SIZE*pieces == TERM_ATOMS_SIZE, "Not a multiple, issue in hash calculation");
    TERM_HASH_TYPE *pt = (TERM_HASH_TYPE*) &term.atoms;
    term.hash = 0;
    for(int i=0; i<pieces; i++, pt++)
    {
        term.hash ^= *pt;
    }
    if(++term.hash == 0)
    {
        term.hash = 1;
    }
    assert(term.hash != 0, "Hashing error: 0 is an invalid hash!");
    return term;
}

