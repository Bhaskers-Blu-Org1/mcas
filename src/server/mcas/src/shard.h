/*
  Copyright [2017-2019] [IBM Corporation]
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
  http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/
#ifndef __mcas_SHARD_H__
#define __mcas_SHARD_H__

#ifdef __cplusplus

#include "fabric_transport.h"

#include <api/ado_itf.h>
#include <api/components.h>
#include <api/fabric_itf.h>
#include <api/kvindex_itf.h>
#include <api/kvstore_itf.h>

#include "connection_handler.h"
#include "mcas_config.h"
#include "pool_manager.h"
#include "task_key_find.h"
#include "types.h"
#include <common/cpu.h>
#include <common/exceptions.h>
#include <common/logging.h>
#include <list>
#include <set>
#include <thread>
#include <unordered_map>

namespace mcas {
class Connection_handler;

/* Adapter point */
using Shard_transport = Fabric_transport;

class Shard : public Shard_transport {

private:
  static constexpr size_t TWO_STAGE_THRESHOLD = KiB(8); /* above this two stage protocol is used */
  static constexpr size_t ADO_MAP_RESERVE = 2048;

private:
  struct lock_info_t {
    Component::IKVStore::pool_t pool;
    Component::IKVStore::key_t key;
    int count;
    size_t value_size;
  };

  using pool_t = Component::IKVStore::pool_t;
  using buffer_t = Shard_transport::buffer_t;
  using index_map_t = std::unordered_map<pool_t, Component::IKVIndex *>;
  using locked_value_map_t = std::unordered_map<const void *, lock_info_t>;
  using task_list_t = std::list<Shard_task *>;

  unsigned option_DEBUG;

  const std::string _default_ado_path;
  const std::string _default_ado_plugin;

public:
  Shard(int core, unsigned int port, const std::string provider,
        const std::string device, const std::string net,
        const std::string backend, const std::string index,
        const std::string pci_addr, const std::string pm_path,
        const std::string dax_config, const std::string default_ado_path,
        const std::string default_ado_plugin, unsigned debug_level,
        bool forced_exit, const std::string ado_cores, float ado_core_num)
      : Shard_transport(provider, net, port),
        _default_ado_path(default_ado_path),
        _default_ado_plugin(default_ado_plugin),
        _forced_exit(forced_exit),
        _core(core),
        _thread(&Shard::thread_entry, this, backend, index, pci_addr,
                dax_config, pm_path, debug_level, ado_cores, ado_core_num) {
    option_DEBUG = mcas::Global::debug_level = debug_level;
    _ado_map.reserve(ADO_MAP_RESERVE);
  }

  ~Shard() {
    _thread_exit = true;
    /* TODO: unblock */
    _thread.join();

    assert(_i_kvstore);
    _i_kvstore->release_ref();

    if (_i_ado_mgr)
      _i_ado_mgr->release_ref();

    if (_index_map) {
      for (auto i : *_index_map) {
        assert(i.second);
        i.second->release_ref();
      }
      delete _index_map;
    }
  }

  bool exited() const { return _thread_exit; }

private:
  void thread_entry(const std::string &backend, const std::string &index,
                    const std::string &pci_addr, const std::string &dax_config,
                    const std::string &pm_path, unsigned debug_level,
                    const std::string ado_cores, float ado_core_num);

  void add_locked_value(const pool_t pool_id, Component::IKVStore::key_t key,
                        void *target, size_t target_len);

  void release_locked_value(const void *target);

  void initialize_components(const std::string &backend,
                             const std::string &index,
                             const std::string &pci_addr,
                             const std::string &dax_config,
                             const std::string &pm_path, unsigned debug_level,
                             const std::string ado_cores,
                             float ado_core_number);

  void check_for_new_connections();

  void main_loop();

  void process_message_pool_request(Connection_handler *handler,
                                    Protocol::Message_pool_request *msg);

  void process_message_IO_request(Connection_handler *handler,
                                  Protocol::Message_IO_request *msg);

  void process_info_request(Connection_handler *handler,
                            Protocol::Message_INFO_request *msg);

  void process_ado_request(Connection_handler *handler,
                           Protocol::Message_ado_request *msg);

  void process_put_ado_request(Connection_handler *handler,
                               Protocol::Message_put_ado_request *msg);


  void process_messages_from_ado();

  status_t process_configure(Protocol::Message_IO_request *msg);

  void process_tasks(unsigned &idle);

  Component::IKVIndex *lookup_index(const pool_t pool_id) {
    if (_index_map) {
      auto search = _index_map->find(pool_id);
      if (search == _index_map->end())
        return nullptr;
      return search->second;
    } else
      return nullptr;
  }

  void add_index_key(const pool_t pool_id, const std::string &k) {
    auto index = lookup_index(pool_id);
    if (index)
      index->insert(k);
  }

  void remove_index_key(const pool_t pool_id, const std::string &k) {
    auto index = lookup_index(pool_id);
    if (index)
      index->erase(k);
  }

  inline void add_task_list(Shard_task *task) { _tasks.push_back(task); }

  inline size_t session_count() const { return _handlers.size(); }

private:
  bool ado_enabled() const { return _i_ado_mgr != nullptr; }
  
  status_t conditional_bootstrap_ado_process(Connection_handler* handler,
                                             Component::IKVStore::pool_t pool_id,
                                             Component::IADO_proxy *& ado);
  
  /* per-shard statistics */
  Component::IMCAS::Shard_stats _stats alignas(8);

  void dump_stats() {
    PINF("------------------------------------------------");
    PINF("| Shard Statistics                             |");
    PINF("------------------------------------------------");
    PINF("PUT count          : %lu", _stats.op_put_count);
    PINF("GET count          : %lu", _stats.op_get_count);
    PINF("PUT_DIRECT count   : %lu", _stats.op_put_direct_count);
    PINF("GET 2-stage count  : %lu", _stats.op_get_twostage_count);
    PINF("ERASE count        : %lu", _stats.op_erase_count);
    PINF("ADO count          : %lu (enabled=%s)", _stats.op_ado_count,
         ado_enabled() ? "yes" : "no");
    PINF("Failed count       : %lu", _stats.op_failed_request_count);
    PINF("Session count      : %lu", session_count());
    PINF("------------------------------------------------");
  }

private:
  struct work_request_t {
    Component::IKVStore::pool_t pool;
    Component::IKVStore::key_t key_handle;
    Component::IKVStore::lock_type_t lock_type;
    uint64_t request_id; /* original client request */
    uint32_t flags;

    inline bool is_async() const { return flags & Component::IMCAS::ADO_FLAG_ASYNC; }
  };

  class Work_request_allocator {

  private:
    static constexpr size_t NUM_ELEMENTS = WORK_REQUEST_ALLOCATOR_COUNT; 
    std::vector<work_request_t*> _free;
    std::vector<work_request_t*> _all;

  public:
    Work_request_allocator() {
      for(size_t i=0;i<NUM_ELEMENTS;i++) {
        work_request_t * wr = reinterpret_cast<work_request_t*>(aligned_alloc(64, sizeof(work_request_t)));
        _free.push_back(wr);
        _all.push_back(wr);
      }
    }

    virtual ~Work_request_allocator() {
      for(auto wr: _all)
        free(wr);
    }

    inline work_request_t * allocate() {
      if(_free.size() == 0)
        throw General_exception("Work_request_allocator exhausted");
      auto wr = _free.back();
      _free.pop_back();
      return wr;
    }

    inline void free_wr(work_request_t* wr) {
      _free.push_back(wr);
    }

  } _wr_allocator;

  using ado_map_t = std::unordered_map<Component::IKVStore::pool_t,
                             std::pair<Component::IADO_proxy*,Connection_handler*>>;

  using work_request_key_t = uint64_t;

  static inline work_request_t *request_key_to_record(work_request_key_t key) {
    return reinterpret_cast<work_request_t *>(key);
  }

  index_map_t*                     _index_map = nullptr;
  bool                             _thread_exit = false;
  bool                             _store_requires_flush = false;
  bool                             _forced_exit;
  unsigned                         _core;
  std::thread                      _thread;
  size_t                           _max_message_size;
  Component::IKVStore*             _i_kvstore;
  Component::IADO_manager_proxy*   _i_ado_mgr = nullptr;
  ado_map_t                        _ado_map;
  std::vector<Connection_handler*> _handlers;
  locked_value_map_t               _locked_values;
  task_list_t                      _tasks;
  std::set<work_request_key_t>     _outstanding_work;
  std::vector<work_request_t*>     _failed_async_requests;
};

} // namespace mcas

#endif

#endif // __SHARD_HPP__
