/*
 * opencog/learning/moses/moses/scoring.h
 *
 * Copyright (C) 2002-2008 Novamente LLC
 * All Rights Reserved
 *
 * Written by Moshe Looks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _MOSES_SCORING_H
#define _MOSES_SCORING_H

#include <iostream>
#include <fstream>

#include <opencog/util/lru_cache.h>

#include <opencog/comboreduct/reduct/reduct.h>
#include <opencog/comboreduct/combo/eval.h>
#include <opencog/comboreduct/combo/table.h>
#include <opencog/comboreduct/reduct/meta_rules.h>

#include "using.h"
#include "representation.h"
#include "types.h"
#include "ant_scoring.h"

namespace moses
{

using opencog::sqr;

#define NEG_INFINITY INT_MIN
 
typedef float fitness_t;

//LRU additions (maybe use IS_FE_LRU_CACHE flag)
#define MOSES_TREE_CACHE_SIZE 1000000

double information_theoretic_bits(const eda::field_set& fs);

/**
 * Compute the score of a boolean function in term of hamming distance
 * of its output and the output of the entire truth table of the
 * indended function.
 */
struct logical_score : public unary_function<combo_tree, int> {
    template<typename Scoring>
    logical_score(const Scoring& score, int a, opencog::RandGen& _rng)
            : target(score, a, _rng), arity(a), rng(_rng) { }

    int operator()(const combo_tree& tr) const;

    combo::truth_table target;
    int arity;
    opencog::RandGen& rng;
};

/**
 * Like logical_score but on behaviors (features). Each feature
 * corresponds to an input tuple, 0 if the output of the candidate
 * matches the output of the intended function (lower is better).
 */
struct logical_bscore : public unary_function<combo_tree, behavioral_score> {
    template<typename Scoring>
    logical_bscore(const Scoring& score, int a, opencog::RandGen& _rng)
            : target(score, a, _rng), arity(a), rng(_rng) { }

    behavioral_score operator()(const combo_tree& tr) const;

    combo::truth_table target;
    int arity;
    opencog::RandGen& rng;
};

struct contin_score : public unary_function<combo_tree, score_t> {
    template<typename Scoring>
    contin_score(const Scoring& score,
                 const RndNumTable& r,
                 opencog::RandGen& _rng)
            : target(score, r), rands(r), rng(_rng) { }

    contin_score(const combo::contin_table& t,
                 const RndNumTable& r,
                 opencog::RandGen& _rng)
        : target(t),rands(r),rng(_rng) { }

    score_t operator()(const combo_tree& tr) const;

    combo::contin_table target;
    RndNumTable rands;
    opencog::RandGen& rng;
};

struct contin_score_sqr : public unary_function<combo_tree,score_t> {
    template<typename Scoring>
    contin_score_sqr(const Scoring& score,
                     const RndNumTable& r,
                     opencog::RandGen& _rng)
        : target(score,r),rands(r),rng(_rng) { }
    
    contin_score_sqr(const combo::contin_table& t,
                     const RndNumTable& r,
                     opencog::RandGen& _rng)
        : target(t),rands(r),rng(_rng) { }
    
    score_t operator()(const combo_tree& tr) const;
    
    combo::contin_table target;
    RndNumTable rands;
    opencog::RandGen& rng;
};

struct contin_bscore : public unary_function<combo_tree, behavioral_score> {
    template<typename Scoring>
    contin_bscore(const Scoring& score,
                  const RndNumTable& r,
                  opencog::RandGen& _rng)
        : target(score, r), rands(r), rng(_rng) { }

    contin_bscore(const combo::contin_table& t,
                  const RndNumTable& r,
                  opencog::RandGen& _rng)
        : target(t), rands(r), rng(_rng) { }

    behavioral_score operator()(const combo_tree& tr) const;

    combo::contin_table target;
    RndNumTable rands;
    opencog::RandGen& rng;
};

/**
 * Calculate the log of the density probability dP(D|M), given the sum
 * squared error of D vs M.
 * D represents the data, for instance a contin_table.
 * M represent the model, i.e. a Combo program.
 * Assuming M's outputs describe Guassians of mean M(x) and variance v, 
 * dP(D|M) = Prod_{x\in D} (2*Pi*v)^(-1/2) exp(-(M(x)-D(x))^2/(2*v))
 * = Sum_{x\in D} log((2*Pi*v)^(-1/2)) + log(exp(-(M(x)-D(x))^2/(2*v)))
 * = Sum_{x\in D} log((2*Pi*v)^(-1/2)) -(M(x)-D(x))^2/(2*v)
 * = |D|*log((2*Pi*v)^(-1/2)) -1/(2*v)*Sum_{x\in D} (M(x)-D(x))^2
 *
 * @param sse  sum squared error
 * @param ds   |D|
 * @param v    variance of the Guassian distribution of M's outputs
 * @return     dP(D|M)
*/
struct LogPDM {
    LogPDM(float v) : variance(v) {
        var_term = variance>0 ? 1/sqrt(log(2*PI*variance)) : 0;        
    }
    score_t operator()(score_t sse, unsigned int ds) const {
        return (score_t)ds*var_term - sse/(2*variance);
    }
    float variance;
    score_t var_term; // to speed up computation, precomputes
                      // 1/sqrt(log(2*PI*variance))
};

struct occam_contin_score : public unary_function<combo_tree,score_t> {
    template<typename Scoring>
    occam_contin_score(const Scoring& score,
                       const RndNumTable& r,
                       float v,
                       float alphabet_size,
                       opencog::RandGen& _rng)
        : target(score,r), rands(r), variance(v), logPDM(v), rng(_rng) {
        alphabet_size_log = log((double)alphabet_size);    
    }

    occam_contin_score(const combo::contin_table& t,
                       const RndNumTable& r,
                       float v,
                       float alphabet_size,
                       opencog::RandGen& _rng)
        : target(t), rands(r), variance(v), logPDM(v), rng(_rng) {
        alphabet_size_log = log((double)alphabet_size);
    }

    score_t operator()(const combo_tree& tr) const;

    combo::contin_table target;
    RndNumTable rands;
    score_t variance;
    LogPDM logPDM;
    score_t alphabet_size_log;
    opencog::RandGen& rng;
};

struct occam_contin_bscore : public unary_function<combo_tree, behavioral_score> {
    template<typename Scoring>
    occam_contin_bscore(const Scoring& score,
                        const RndNumTable& r,
                        float v,
                        float alphabet_size,
                        opencog::RandGen& _rng)
        : target(score, r), rands(r), variance(v), logPDM(v), rng(_rng) {
        alphabet_size_log = log((double)alphabet_size);
    }

    occam_contin_bscore(const combo::contin_table& t,
                        const RndNumTable& r,
                        float v,
                        float alphabet_size,
                        opencog::RandGen& _rng)
        : target(t), rands(r), variance(v), logPDM(v), rng(_rng) {
        alphabet_size_log = log((double)alphabet_size);
    }

    behavioral_score operator()(const combo_tree& tr) const;

    combo::contin_table target;
    RndNumTable rands;
    score_t variance;
    LogPDM logPDM;
    score_t alphabet_size_log;
    opencog::RandGen& rng;
};

template<typename Scoring>
struct complexity_based_scorer : public unary_function<eda::instance,
                                                       combo_tree_score> {
    complexity_based_scorer(const Scoring& s,
                            representation& rep,
                            opencog::RandGen& _rng)
            : score(s), _rep(&rep), rng(_rng) { }

    combo_tree_score operator()(const eda::instance& inst) const {
        using namespace reduct;

        //cout << "top, got " << _rep->fields().stream(inst) << endl;
        _rep->transform(inst);
        combo_tree tr = _rep->exemplar();

        //reduct::apply_rule(reduct::downwards(reduct::remove_null_vertices()),tr);
        //if simplification of all trees is enabled, we should instead do
        //apply_rule(downwards(remove_null_vertices()),tr);
        //clean_and_full_reduce(tr);
        clean_and_full_reduce(tr, rng);

        //to maybe speed this up, we can score directly on the exemplar,
        //and have complexity(tr) ignore the null vertices

#ifdef DEBUG_INFO
        std::cout << "scoring " << tr << endl;
        /*std::cout << "scoring " << tr << " -> " << score(tr)
        << " " << complexity(tr.begin()) << std::endl;*/
#endif

        return combo_tree_score(score(tr), complexity(tr.begin()));
    }

    Scoring score;
protected:
    representation* _rep;
    opencog::RandGen& rng;
};

// template<typename Scoring>
// struct count_based_scorer : public unary_function<eda::instance, 
//                                                   combo_tree_score> {
//     count_based_scorer(const Scoring& s,
//                        representation* rep,
//                        int base_count,
//                        opencog::RandGen& _rng)
//         : score(s), _base_count(base_count), _rep(rep), rng(_rng), 
//           treecache(MOSES_TREE_CACHE_SIZE, score) {}

//     int get_misses() {
//         return treecache->get_number_of_evaluations();
//     }
    
//     combo_tree_score operator()(const eda::instance& inst) const {
// #ifdef DEBUG_INFO
//         std::cout << "transforming " << _rep->fields().stream(inst) << std::endl;
// #endif
//         _rep->transform(inst);

//         combo_tree tr;

//         try {
//             tr = _rep->get_clean_exemplar();
//         } catch (...) {
//             std::cout << "get_clean_exemplar threw" << std::endl;
//             return worst_possible_score;
//         }

//         // sequential(clean_reduction(),logical_reduction())(tr,tr.begin());
//         // std::cout << "OK " << tr << std::endl;
//         // reduct::clean_and_full_reduce(tr);
//         // reduct::clean_reduce(tr);
//         // reduct::contin_reduce(tr,rng);

//         score_t sc = treecache(tr);
//         // score_t sc = score(tr);

//         combo_tree_score ts = combo_tree_score(sc,
//                                                - int(_rep->fields().count(inst))
//                                                + _base_count);        
// #ifdef DEBUG_INFO
//         std::cout << "OKK " << tr << std::endl;
//         std::cout << "Score:" << ts << std::endl;
// #endif
//         return ts;
//     }
    
//     Scoring score;
//     int _base_count;
//     representation* _rep;
// protected:
//     opencog::RandGen& rng;
//     opencog::lru_cache<cached_scoring_wrapper<Scoring> > treecache;
// };

template<typename Scoring>
struct count_based_scorer : public unary_function<eda::instance, 
                                                  combo_tree_score> {
    count_based_scorer(const Scoring& s,
                       representation* rep,
                       int base_count,
                       opencog::RandGen& _rng)
        : score(s), _base_count(base_count), _rep(rep), rng(_rng),
          treecache(MOSES_TREE_CACHE_SIZE, score) {}

    int get_misses() {
        return treecache.get_number_of_evaluations();
    }
    
    combo_tree_score operator()(const eda::instance& inst) const {
        OC_ASSERT(_rep);

#ifdef DEBUG_INFO
        std::cout << "transforming " << _rep->fields().stream(inst) << std::endl;
#endif
        _rep->transform(inst);

        combo_tree tr;

        try {
            tr = _rep->get_clean_exemplar();
        } catch (...) {
            std::cout << "get_clean_exemplar threw" << std::endl;
            return worst_possible_score;
        }

        // sequential(clean_reduction(),logical_reduction())(tr,tr.begin());
        // std::cout << "OK " << tr << std::endl;
        // reduct::clean_and_full_reduce(tr);
        // reduct::clean_reduce(tr);
        // reduct::contin_reduce(tr,rng);

        combo_tree_score ts = combo_tree_score(treecache(tr),
                                               - int(_rep->fields().count(inst))
                                               + _base_count);
        
//        combo_tree_score ts = combo_tree_score(score(tr),
//                                   -int(_rep->fields().count(inst))
//                                   + _base_count);
#ifdef DEBUG_INFO
        std::cout << "OKK " << tr << std::endl;
        std::cout << "Score:" << ts << std::endl;
#endif
        return ts;
    }
    
    Scoring score;
    int _base_count;
    representation* _rep;
protected:
    opencog::RandGen& rng;
    mutable opencog::lru_cache<Scoring> treecache;
};

/**
 * return true if x dominates y
 *        false if y dominates x
 *        indeterminate otherwise
 */
tribool dominates(const behavioral_score& x, const behavioral_score& y);


/**
 * For all candidates c in [from, to), insert c in dst iff 
 * no element of dst dominates c
 */
//this may turn out to be too slow...
template<typename It, typename Set>
void merge_nondominating(It from, It to, Set& dst)
{
    for (;from != to;++from) {
        // std::cout << "ook " << std::distance(from,to) << std::endl; // PJ
        bool nondominated = true;
        for (typename Set::iterator it = dst.begin();it != dst.end();) {
            tribool dom = dominates(from->second, it->second);
            if (dom) {
                dst.erase(it++);
            } else if (!dom) {
                nondominated = false;
                break;
            } else {
                ++it;
            }
        }
        if (nondominated)
            dst.insert(*from);
    }
}

/**
 * score calculated based on the behavioral score. Useful to avoid
 * redundancy of code and computation in case there is a cache over
 * bscore. The score is calculated as the minus of the sum of the
 * bscore over all features, that is:
 * score = - sum_f BScore(f),
 */
template<typename BScore>
struct bscore_based_score : public unary_function<combo_tree, score_t>
{
    bscore_based_score(const BScore& bs) : bscore(bs) {}
    score_t operator()(const combo_tree& tr) const {
        try {
            behavioral_score bs = bscore(tr);
            return -std::accumulate(bs.begin(), bs.end(), 0);
        } catch (...) {
            stringstream ss;
            ss << "The following candidate has failed to be evaluated: " << tr;
            logger().warn(ss.str());
            return get_score(worst_possible_score);
        }
    }
    const BScore& bscore;
};

} //~namespace moses

#endif
