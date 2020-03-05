// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef _SPTAG_KDT_INDEX_H_
#define _SPTAG_KDT_INDEX_H_

#include "../Common.h"
#include "../VectorIndex.h"

#include "../Common/CommonUtils.h"
#include "../Common/DistanceUtils.h"
#include "../Common/QueryResultSet.h"
#include "../Common/Dataset.h"
#include "../Common/WorkSpace.h"
#include "../Common/WorkSpacePool.h"
#include "../Common/RelativeNeighborhoodGraph.h"
#include "../Common/KDTree.h"
#include "../Common/Labelset.h"
#include "inc/Helper/SimpleIniReader.h"
#include "inc/Helper/StringConvert.h"
#include "inc/Helper/ThreadPool.h"

#include <functional>
#include <shared_mutex>

namespace SPTAG
{

    namespace Helper
    {
        class IniReader;
    }

    namespace KDT
    {
        template<typename T>
        class Index : public VectorIndex
        {
            class RebuildJob : public Helper::ThreadPool::Job {
            public:
                RebuildJob(VectorIndex* p_index, COMMON::KDTree* p_tree, COMMON::RelativeNeighborhoodGraph* p_graph) : m_index(p_index), m_tree(p_tree), m_graph(p_graph) {}
                void exec() {
                    m_tree->Rebuild<T>(m_index);
                }
            private:
                VectorIndex* m_index;
                COMMON::KDTree* m_tree;
                COMMON::RelativeNeighborhoodGraph* m_graph;
            };

        private:
            // data points
            COMMON::Dataset<T> m_pSamples;

            // KDT structures. 
            COMMON::KDTree m_pTrees;

            // Graph structure
            COMMON::RelativeNeighborhoodGraph m_pGraph;

            std::string m_sKDTFilename;
            std::string m_sGraphFilename;
            std::string m_sDataPointsFilename;
            std::string m_sDeleteDataPointsFilename;

            int m_addCountForRebuild;
            float m_fDeletePercentageForRefine;
            std::mutex m_dataAddLock; // protect data and graph
            std::shared_timed_mutex m_dataDeleteLock;
            COMMON::Labelset m_deletedID;
            
            std::unique_ptr<COMMON::WorkSpacePool> m_workSpacePool;
            Helper::ThreadPool m_threadPool;
            int m_iNumberOfThreads;

            DistCalcMethod m_iDistCalcMethod;
            float(*m_fComputeDistance)(const T* pX, const T* pY, DimensionType length);
 
            int m_iMaxCheck;
            int m_iThresholdOfNumberOfContinuousNoBetterPropagation;
            int m_iNumberOfInitialDynamicPivots;
            int m_iNumberOfOtherDynamicPivots;
        public:
            Index()
            {
#define DefineKDTParameter(VarName, VarType, DefaultValue, RepresentStr) \
                VarName = DefaultValue; \

#include "inc/Core/KDT/ParameterDefinitionList.h"
#undef DefineKDTParameter
                
                m_pSamples.SetName("Vector");
                m_fComputeDistance = COMMON::DistanceCalcSelector<T>(m_iDistCalcMethod);
            }

            ~Index() {}

            inline SizeType GetNumSamples() const { return m_pSamples.R(); }
            inline DimensionType GetFeatureDim() const { return m_pSamples.C(); }
            
            inline int GetCurrMaxCheck() const { return m_iMaxCheck; }
            inline int GetNumThreads() const { return m_iNumberOfThreads; }
            inline DistCalcMethod GetDistCalcMethod() const { return m_iDistCalcMethod; }
            inline IndexAlgoType GetIndexAlgoType() const { return IndexAlgoType::KDT; }
            inline VectorValueType GetVectorValueType() const { return GetEnumValueType<T>(); }
            
            inline float ComputeDistance(const void* pX, const void* pY) const { return m_fComputeDistance((const T*)pX, (const T*)pY, m_pSamples.C()); }
            inline const void* GetSample(const SizeType idx) const { return (void*)m_pSamples[idx]; }
            inline bool ContainSample(const SizeType idx) const { return !m_deletedID.Contains(idx); }
            inline bool NeedRefine() const { return m_deletedID.Count() >= (size_t)(GetNumSamples() * m_fDeletePercentageForRefine); }
            std::shared_ptr<std::vector<std::uint64_t>> BufferSize() const
            {
                std::shared_ptr<std::vector<std::uint64_t>> buffersize(new std::vector<std::uint64_t>);
                buffersize->push_back(m_pSamples.BufferSize());
                buffersize->push_back(m_pTrees.BufferSize());
                buffersize->push_back(m_pGraph.BufferSize());
                buffersize->push_back(m_deletedID.BufferSize());
                return std::move(buffersize);
            }

            ErrorCode SaveConfig(std::ostream& p_configout) const;
            ErrorCode SaveIndexData(const std::string& p_folderPath);
            ErrorCode SaveIndexData(const std::vector<std::ostream*>& p_indexStreams);

            ErrorCode LoadConfig(Helper::IniReader& p_reader);
            ErrorCode LoadIndexData(const std::string& p_folderPath);
            ErrorCode LoadIndexDataFromMemory(const std::vector<ByteArray>& p_indexBlobs);

            ErrorCode BuildIndex(const void* p_data, SizeType p_vectorNum, DimensionType p_dimension);
            ErrorCode SearchIndex(QueryResult &p_query, bool p_searchDeleted = false) const;
            ErrorCode RefineSearchIndex(QueryResult &p_query, bool p_searchDeleted = false) const;
            ErrorCode AddIndex(const void* p_data, SizeType p_vectorNum, DimensionType p_dimension, std::shared_ptr<MetadataSet> p_metadataSet, bool p_withMetaIndex = false);
            ErrorCode DeleteIndex(const void* p_vectors, SizeType p_vectorNum);
            ErrorCode DeleteIndex(const SizeType& p_id);
            ErrorCode SearchTree(QueryResult& p_results) const;

            ErrorCode SetParameter(const char* p_param, const char* p_value);
            std::string GetParameter(const char* p_param) const;

            ErrorCode RefineIndex(const std::string& p_folderPath);
            ErrorCode RefineIndex(const std::vector<std::ostream*>& p_indexStreams);
            ErrorCode RefineIndex(std::shared_ptr<VectorIndex>& p_newIndex);

        private:
            void SearchIndexWithDeleted(COMMON::QueryResultSet<T> &p_query, COMMON::WorkSpace &p_space) const;
            void SearchIndexWithoutDeleted(COMMON::QueryResultSet<T> &p_query, COMMON::WorkSpace &p_space) const;
        };
    } // namespace KDT
} // namespace SPTAG

#endif // _SPTAG_KDT_INDEX_H_
