/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   RAJA header file containing constructs used to run forallN
 *          traversals on GPU with CUDA.
 *
 ******************************************************************************
 */

#ifndef RAJA_policy_cuda_nested_internal_HPP
#define RAJA_policy_cuda_nested_internal_HPP

#include "RAJA/config.hpp"
#include "camp/camp.hpp"
#include "RAJA/pattern/nested.hpp"

#if defined(RAJA_ENABLE_CUDA)

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2016, Lawrence Livermore National Security, LLC.
//
// Produced at the Lawrence Livermore National Laboratory
//
// LLNL-CODE-689114
//
// All rights reserved.
//
// This file is part of RAJA.
//
// For additional details, please also read RAJA/LICENSE.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the disclaimer below.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the disclaimer (as noted below) in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of the LLNS/LLNL nor the names of its contributors may
//   be used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY,
// LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

#include <cassert>
#include <climits>

#include "RAJA/config.hpp"
#include "RAJA/util/defines.hpp"
#include "RAJA/util/types.hpp"

#include "RAJA/pattern/nested/For.hpp"
#include "RAJA/pattern/nested/Lambda.hpp"

#include "RAJA/policy/cuda/MemUtils_CUDA.hpp"
#include "RAJA/policy/cuda/policy.hpp"

#include "RAJA/internal/ForallNPolicy.hpp"
#include "RAJA/internal/LegacyCompatibility.hpp"


namespace RAJA
{


/*!
 * Policy for For<>, executes loop iteration by distributing them over threads.
 * This does no (additional) work-sharing between thread blocks.
 */

struct cuda_thread_exec{};




/*!
 * Policy for For<>, executes loop iteration by distributing iterations
 * exclusively over blocks.
 */

struct cuda_block_exec{};




/*!
 * Policy for For<>, executes loop iteration by distributing work over
 * physical blocks and executing sequentially within blocks.
 */

template<size_t num_blocks>
struct cuda_block_seq_exec{};





/*!
 * Policy for For<>, executes loop iteration by distributing them over threads
 * and blocks, but limiting the number of threads to num_threads.
 */
template<size_t num_threads>
struct cuda_threadblock_exec{};


namespace nested
{


namespace internal
{


struct LaunchDim {

  int blocks;
  int threads;

  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr
  LaunchDim() : blocks(1), threads(1) {}


  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr
  LaunchDim(int b, int t) : blocks(b), threads(t){}


  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr
  LaunchDim(LaunchDim const &c) : blocks(c.blocks), threads(c.threads) {}



  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr
  LaunchDim maximum(LaunchDim const & c) const {
    return LaunchDim{
      blocks > c.blocks ? blocks : c.blocks,
      threads > c.threads ? threads : c.threads};
  }

  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr
  LaunchDim operator*(LaunchDim const & c) const {
    return LaunchDim{blocks*c.blocks, threads*c.threads};
  }

};

struct CudaLaunchLimits {
  LaunchDim max_dims;
  LaunchDim physical_dims;
};





template<camp::idx_t ArgumentId, typename ExecPolicy>
struct CudaIndexCalc_Policy;


template<camp::idx_t ArgumentId>
struct CudaIndexCalc_Policy<ArgumentId, cuda_block_exec>{

  int len;

  template<typename SegmentTuple>
  RAJA_INLINE
  RAJA_HOST_DEVICE
  CudaIndexCalc_Policy(SegmentTuple const &segments, LaunchDim const &) :
  len((int)(camp::get<ArgumentId>(segments).end()-camp::get<ArgumentId>(segments).begin()))
  {
  }

  RAJA_INLINE
  RAJA_HOST_DEVICE
  int numLogicalBlocks() const {
    return (int)len;
  }
  
	RAJA_INLINE
  RAJA_HOST_DEVICE
  int numLogicalThreads() const {
    return 1;
  }

  template<typename Data>
  RAJA_INLINE
  RAJA_DEVICE
  bool assignIndex(Data &data, int *block, int *thread){

    // Compute our index, and strip off the block index
    int i = (*block) % len;
    (*block) /= len;

    // Assign our computed index to the tuple
    auto begin = camp::get<ArgumentId>(data.segment_tuple).begin();
    data.template assign_index<ArgumentId>(*(begin+i));

    return true;
  }

};





template<camp::idx_t ArgumentId>
struct CudaIndexCalc_Policy<ArgumentId, cuda_thread_exec>{

  int len;

  template<typename SegmentTuple>
  RAJA_INLINE
  RAJA_HOST_DEVICE
  CudaIndexCalc_Policy(SegmentTuple const &segments, LaunchDim const &) :
  len((int)(camp::get<ArgumentId>(segments).end()-camp::get<ArgumentId>(segments).begin()))
  {
  }

  RAJA_INLINE
  RAJA_HOST_DEVICE
  int numLogicalBlocks() const {
    return 1;
  }

  RAJA_INLINE
  RAJA_HOST_DEVICE
  int numLogicalThreads() const {
    return (int)len;
  }


  template<typename Data>
  RAJA_INLINE
  RAJA_DEVICE
  bool assignIndex(Data &data, int *block, int *thread){

    // Compute our index, and strip off the block index
    int i = (*thread) % len;
    (*thread) /= len;

    // Assign our computed index to the tuple
    auto begin = camp::get<ArgumentId>(data.segment_tuple).begin();
    data.template assign_index<ArgumentId>(*(begin+i));

    return true;
  }

};



template<camp::idx_t ArgumentId, size_t num_threads_max>
struct CudaIndexCalc_Policy<ArgumentId, cuda_threadblock_exec<num_threads_max>>{

  int len;
  int num_threads;
  int num_blocks;

  template<typename SegmentTuple>
  RAJA_INLINE
  RAJA_HOST_DEVICE
  CudaIndexCalc_Policy(SegmentTuple const &segments, LaunchDim const &) :
    len(camp::get<ArgumentId>(segments).end()-camp::get<ArgumentId>(segments).begin()),
    num_threads(num_threads_max < len ? num_threads_max : len),
    num_blocks(len/num_threads)
  {
    if(num_threads*num_blocks < len){
      num_blocks ++;
    }
  }

  RAJA_INLINE
  RAJA_HOST_DEVICE
  int numLogicalBlocks() const {
    return num_blocks;
  }

  RAJA_INLINE
  RAJA_HOST_DEVICE
  int numLogicalThreads() const {
    return num_threads;
  }


  template<typename Data>
  RAJA_INLINE
  RAJA_DEVICE
  bool assignIndex(Data &data, int *block, int *thread){

    // Compute our index, and strip off the thread index
    int block_i = (*block) % num_blocks;
    (*block) /= num_blocks;

    int thread_i = (*thread) % num_threads;
    (*thread) /= num_threads;

    int i = block_i*num_threads + thread_i;

    // Assign our computed index to the tuple
    if(i < len){
      auto begin = camp::get<ArgumentId>(data.segment_tuple).begin();
      data.template assign_index<ArgumentId>(*(begin+i));
      return true;
    }
    // our i is out of bounds
    return false;
  }

};





template<typename SegmentTuple, typename ExecPolicies, typename RangeList>
struct CudaIndexCalc;


template<typename SegmentTuple, typename ... IndexPolicies, camp::idx_t ... RangeInts>
struct CudaIndexCalc<SegmentTuple, camp::list<IndexPolicies...>, camp::idx_seq<RangeInts...>>{

  using CalcList = camp::tuple<IndexPolicies...>;

  CalcList calc_list;

  RAJA_INLINE
  RAJA_HOST_DEVICE
  CudaIndexCalc(SegmentTuple const &segment_tuple, LaunchDim const &max_physical) :
    calc_list( camp::make_tuple( (IndexPolicies(segment_tuple, max_physical) )...) )
  {
  }

  RAJA_INLINE
  RAJA_HOST_DEVICE
  LaunchDim computeLogicalDims(){

    // evaluate product of each Calculator's block and thread requirements
    return LaunchDim(numLogicalBlocks(), numLogicalThreads());
  }


  RAJA_INLINE
  RAJA_HOST_DEVICE
  int numLogicalBlocks() const {
    return VarOps::foldl(RAJA::operators::multiplies<int>(),
        camp::get<RangeInts>(calc_list).numLogicalBlocks()...);
  }

  RAJA_INLINE
  RAJA_HOST_DEVICE
  int numLogicalThreads() const {
    return VarOps::foldl(RAJA::operators::multiplies<int>(),
        camp::get<RangeInts>(calc_list).numLogicalThreads()...);
  }

  template<typename Data>
  RAJA_INLINE
  RAJA_DEVICE
  bool assignIndices(Data &data, int const block, int const thread){

    // evaluate each index, passing block_thread through
    // each calculator will trim block_thread appropriately

    int block_tmp = block;
    int thread_tmp = thread;

    bool in_bounds = VarOps::foldl(RAJA::operators::logical_and<bool>(),
        camp::get<RangeInts>(calc_list).assignIndex(data, &block_tmp, &thread_tmp)...);

    if(block_tmp > 0 || thread_tmp > 0){
      return false;
    }


    return in_bounds;
  }

};


/**
 * Empty calculator case.  Needed for SetShmemWindow when no For loops have
 * been issues (ie. just Tile loops)
 */
template<typename SegmentTuple>
struct CudaIndexCalc<SegmentTuple, camp::list<>, camp::idx_seq<>>{

  RAJA_INLINE
  RAJA_HOST_DEVICE
  CudaIndexCalc(SegmentTuple const &, LaunchDim const &)
  {
  }

  RAJA_INLINE
  RAJA_HOST_DEVICE
  LaunchDim computeLogicalDims(){

    // evaluate product of each Calculator's block and thread requirements
    return LaunchDim(1, 1);
  }


  RAJA_INLINE
  RAJA_HOST_DEVICE
  int numLogicalBlocks() const {
    return 1;
  }

  RAJA_INLINE
  RAJA_HOST_DEVICE
  int numLogicalThreads() const {
    return 1;
  }

  template<typename Data>
  RAJA_INLINE
  RAJA_DEVICE
  bool assignIndices(Data &, int , int ){
    return false;
  }

};

template<typename SegmentTuple>
using CudaIndexCalc_Terminator = CudaIndexCalc<SegmentTuple, camp::list<>, camp::idx_seq<>>;


template<typename IndexCalcBase, typename CalcN>
struct CudaIndexCalc_Extender;


template<typename SegmentTuple, typename ... CalcPolicies, camp::idx_t ... RangeInts, typename CalcN>
struct CudaIndexCalc_Extender<CudaIndexCalc<SegmentTuple, camp::list<CalcPolicies...>, camp::idx_seq<RangeInts...>>, CalcN>{
  using type = CudaIndexCalc<SegmentTuple, camp::list<CalcPolicies..., CalcN>, camp::idx_seq<RangeInts..., sizeof...(RangeInts)>>;
};

template<typename IndexCalcBase, typename CalcN>
using ExtendCudaIndexCalc = typename CudaIndexCalc_Extender<IndexCalcBase, CalcN>::type;








template <typename Policy, typename IndexCalc>
struct CudaStatementExecutor;

template <camp::idx_t idx, camp::idx_t N, typename StmtList, typename IndexCalc>
struct CudaStatementListExecutor;


template <camp::idx_t statement_index, camp::idx_t num_statements, typename StmtList, typename IndexCalc>
struct CudaStatementListExecutor{

  template<typename Data>
  static
  inline
  __device__
  void exec(Data &data, long logical_block){

    // Get the statement we're going to execute
    using statement = camp::at_v<StmtList, statement_index>;

    // Execute this statement
    CudaStatementExecutor<statement, IndexCalc>::exec(data, logical_block);

    // call the next statement in the list
    CudaStatementListExecutor<statement_index+1, num_statements, StmtList, IndexCalc>::exec(data, logical_block);
  }


  template<typename Data>
  static
  RAJA_INLINE
  LaunchDim calculateDimensions(Data const &data, LaunchDim const &max_physical){

    // Compute this statements launch dimensions
    using statement = camp::at_v<StmtList, statement_index>;
    LaunchDim statement_dims = CudaStatementExecutor<statement, IndexCalc>::calculateDimensions(data, max_physical);

    // call the next statement in the list
    LaunchDim next_dims = CudaStatementListExecutor<statement_index+1, num_statements, StmtList, IndexCalc>::calculateDimensions(data, max_physical);

    // Return the maximum of the two
    return statement_dims.maximum(next_dims);
  }


};


/*
 * termination case, a NOP.
 */

template <camp::idx_t num_statements, typename ... Stmts, typename IndexCalc>
struct CudaStatementListExecutor<num_statements,num_statements, StatementList<Stmts...>, IndexCalc> {

  template<typename Data>
  static
  inline
  __device__
  void exec(Data &, long) {}


  template<typename Data>
  static
  RAJA_INLINE
  LaunchDim calculateDimensions(Data const &, LaunchDim const &){

    return LaunchDim();
  }
};



template<typename StmtList, typename IndexCalc, typename Data>
RAJA_DEVICE
RAJA_INLINE
void cuda_execute_statement_list(Data &data, long logical_block){

  CudaStatementListExecutor<0, StmtList::size, StmtList, IndexCalc>::exec(data, logical_block);

}




template<typename StmtList, typename IndexCalc, typename Data>
RAJA_INLINE
LaunchDim cuda_calcdims_statement_list(Data const &data, LaunchDim const &max_physical){

  using StmtListExec = CudaStatementListExecutor<0, StmtList::size, StmtList, IndexCalc>;

  return StmtListExec::calculateDimensions(data, max_physical);
}


template<typename Data, typename ... EnclosedStmts>
RAJA_INLINE
LaunchDim cuda_calculate_logical_dimensions(Data const &data, LaunchDim const &max_physical){

  using index_calc_t = CudaIndexCalc_Terminator<typename Data::segment_tuple_t>;
  using stmt_list_t = StatementList<EnclosedStmts...>;

  return cuda_calcdims_statement_list<stmt_list_t, index_calc_t>(data, max_physical);

}



}  // namespace internal
}  // namespace nested
}  // namespace RAJA

#endif  // closing endif for RAJA_ENABLE_CUDA guard

#endif  // closing endif for header file include guard
