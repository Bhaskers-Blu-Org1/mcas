/*
   Copyright [2019] [IBM Corporation]
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

/*
 * Authors:
 *
 * Daniel G. Waddington (daniel.waddington@ibm.com)
 * Luna Xu (xuluna@ibm.com)
 *
 */

#ifndef __API_ADO_ITF_H__
#define __API_ADO_ITF_H__

#include <api/kvstore_itf.h>
#include <common/errors.h>
#include <common/types.h>
#include <component/base.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Component {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

class SLA; /* to be defined - placeholder only */

/**
 * Component for ADO process plugins
 *
 */
class IADO_plugin : public Component::IBase {
public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0xacb20ef2,0xe796,0x4619,0x845d,0x4e,0x8e,0x6b,0x5c,0x35,0xaa);
  // clang-format on

  /**
   * Inform the plugin of memory mapped into the ADO process (normally pool
   * memory)
   *
   * @param shard_vaddr Virtual address as mapped in shard
   * @param local_vaddr Virtual address as mapped in local ADO process
   * @param len Size of mapping in bytes
   *
   * @return S_OK on success
   */
  virtual status_t register_mapped_memory(void *shard_vaddr, void *local_vaddr,
                                          size_t len) = 0;

  /**
   * Perform ADO operation on a key's value
   *
   * @param key Key for target
   * @param value Area of value memory associated to invoked key-value pair
   * @param value_len Length of value memory in bytes
   * @param in_work_request Open protocol request message
   * @param in_work_request_len Open protocol request message length
   * @param out_work_response Open protocol response message
   * @param out_work_response_len Open protocol response message length
   *
   * @return ADO plugin response
   */
  virtual status_t do_work(
      const uint64_t work_key,
      const std::string &key,
      void * value,
      size_t value_len,
      const void *in_work_request, /* don't use iovec because of non-const */
      const size_t in_work_request_len,
      void *&out_work_response,
      size_t &out_work_response_len) = 0;


  /**
   * Callbacks used by ADO plugin
   */
  struct Callback_table {
    std::function<status_t(const uint64_t work_id,
                           const std::string &key_name,
                           const size_t value_size,
                           void *&out_value_addr)>
    create_key_func;
    
    std::function<status_t(const uint64_t work_id,
                           const std::string &key_name,
                           void *&out_value_addr,
                           size_t &out_value_len)>
    open_key_func;
    
    std::function<status_t(const uint64_t work_id,
                           const std::string &key_name)>
    erase_key_func;

    std::function<status_t(const uint64_t work_id,
                           const std::string& key_name,
                           const size_t new_value_size,
                           void *&out_new_value_addr)>
    resize_value_func;

    std::function<status_t(const uint64_t work_id,
                           const size_t size,
                           const size_t alignment,
                           void *&out_new_addr)>
    allocate_pool_memory_func;

    std::function<status_t(const uint64_t work_id,
                           const size_t size,
                           void * addr)>
    free_pool_memory_func;
    
  };

    
  /**
   * Register callbacks, so the plugin can perform KV-pair operations (sent to
   * shard to perform)
   *
   * @param create_key Callback function to create a new key in the current pool
   * @param erase_key Callback function to erase a key from the current pool
   */
  void register_callbacks(const Callback_table& callbacks) { _cb = callbacks; }
  
  /**
   * Request graceful shutdown. This will be called by ADO process on shutdown.
   *
   *
   * @return S_OK on success
   */
  virtual status_t shutdown() = 0;

protected:
  IADO_plugin::Callback_table _cb; /*< callback functions */
};

/**
 * ADO interface.  This is actually a proxy interface communicating with the
 * external process.
 *
 */
class IADO_proxy : public Component::IBase {
public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0xbbbfa389,0x1665,0x4e5b,0xa1b1,0x3c,0xff,0x4a,0x5e,0xe2,0x63);
  // clang-format on

  enum { /* this should match ado_proto.fbs */
         OP_CREATE = 0,
         OP_OPEN = 1,
         OP_ERASE = 2,
         OP_RESIZE = 3,
         OP_ALLOC = 4,
         OP_FREE = 5,
  };

  using work_id_t = uint64_t; /*< work handle/identifier */

  /* ADO-to-SHARD (and vice versa) protocol */
  virtual status_t bootstrap_ado() = 0;

  /**
   * Send a memory map request to ADO
   *
   * @param token Token from MCAS expose_memory call
   * @param size Size of the memory
   * @param value_vaddr Virtual address as mapped by shard (may be different in
   * ADO)
   *
   * @return S_OK on success
   */
  virtual status_t send_memory_map(uint64_t token, size_t size,
                                   void *value_vaddr) = 0;

  /**
   * Send a work request to the ADO
   *
   * @param work_request_key Unique request identifier
   * @param value_addr Virtual address of value (as mapped by shard)
   * @param value_len Length of value in bytes
   * @param invocation_data Data representing the work request (may be binary or
   * string)
   * @param invocation_len Length of data representing work
   *
   * @return S_OK on success
   */
  virtual status_t send_work_request(const uint64_t work_request_key,
                                     const std::string &work_key_str,
                                     const void *value_addr,
                                     const size_t value_len,
                                     const void *invocation_data,
                                     const size_t invocation_len) = 0;

  /**
   * Check for completion of work
   *
   * @param out_work_request_key [out] Work request identifier of completed work
   * @param out_status [out] Status
   * @param out_response [out] Response data
   * @param out_response_len [out] Size of response data
   *
   * @return True if bytes received
   */
  virtual bool check_work_completions(uint64_t& out_work_request_key,
                                      status_t& out_status,
                                      void *& out_response, /* use ::free to release */
                                      size_t & out_response_len) = 0;

  /**
   * Check for table operations (e.g., create_key)
   *
   * @param out_work_request_key [out] Work request identifier of completed work
   * @param op [out] Operation type (see ado_proto.fbs)
   * @param key [out] Name of corresponding key
   * @param value_len [out] Size in bytes (optional)
   *
   * @return True if op received
   */
  virtual bool check_table_ops(uint64_t &work_request_id,
                               int &op,
                               std::string &key,
                               size_t &value_len,
                               size_t &value_alignment,
                               void *& addr) = 0;


  /**
   * Send response to ADO for table operation
   *
   * @param s Status code
   * @param value_addr [optional] Address of new
   * @param value_len [optional] Length of value in bytes
   *
   */
  virtual void send_table_op_response(const status_t s,
                                      const void *value_addr = nullptr,
                                      size_t value_len = 0) = 0;

  /**
   * Indicate whether ADO has shutdown
   *
   *
   * @return
   */
  virtual bool has_exited() = 0;

  /**
   * Request graceful shutdown
   *
   *
   * @return S_OK on success
   */
  virtual status_t shutdown() = 0;

  /**
   * Get pool id proxy is associated with
   *
   *
   * @return Pool id
   */
  virtual Component::IKVStore::pool_t pool_id() const = 0;

  /**
   * Get ado id proxy is associated with
   *
   *
   * @return ado id
   */
  virtual std::string ado_id() const = 0;

  /**
   * Add a key-value pair for deferred unlock
   *
   * @param work_request_id
   * @param key
   */
  virtual void add_deferred_unlock(const uint64_t work_request_id,
                                   const Component::IKVStore::key_t key) = 0;

  /**
   * Retrive (and clear) keys that need to be unlock on associated pool
   *
   * @param work_request_id
   * @param keys Out vector of keys
   */
  virtual void
  get_deferred_unlocks(const uint64_t work_request_id,
                       std::vector<Component::IKVStore::key_t> &keys) = 0;
};

/**
 * ADO manager interface.  This is actually a proxy interface communicating with
 * an external process. The ADO manager has a "global" view of the system and
 * can coordinate / schedule resources that are being consumed by the ADO
 * processes.
 */
class IADO_manager_proxy : public Component::IBase {
public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0xaaafa389,0x1665,0x4e5b,0xa1b1,0x3c,0xff,0x4a,0x5e,0xe2,0x63);
  // clang-format on

  using shared_memory_token_t =
      uint64_t; /*< token identifying shared memory for mcas module */

  /**
   * Launch ADO process.  This method must NOT block.
   *
   * @param filename Location of the executable
   * @param args Command line arguments to pass
   * @param shm_token Token to pass to ADO to use to map value memory into
   * process space.
   * @param value_memory_numa_zone NUMA zone to which the value memory resides
   * @param sla Placeholder for some type of SLA/QoS requirements specification.
   *
   * @return Proxy interface, with reference count 1. Use release_ref() to
   * destroy.
   */
  virtual IADO_proxy *create(Component::IKVStore::pool_t pool_id,
                             const std::string &filename,
                             std::vector<std::string> &args,
                             numa_node_t value_memory_numa_zone,
                             SLA *sla = nullptr) = 0;

  /**
   * Wait for process to exit.
   *
   * @param ado_proxy Handle to proxy object
   *
   * @return S_OK on success or E_BUSY.
   */
  virtual bool has_exited(IADO_proxy *ado_proxy) = 0;

  /**
   * Shutdown ADO process
   *
   * @param ado Interface pointer to proxy
   *
   * @return S_OK on success
   */
  virtual status_t shutdown(IADO_proxy *ado) = 0;
};

class IADO_manager_proxy_factory : public Component::IBase {
public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0xfacfa389,0x1665,0x4e5b,0xa1b1,0x3c,0xff,0x4a,0x5e,0xe2,0x63);
  // clang-format on

  virtual IADO_manager_proxy *create(unsigned debug_level, int shard,
                                     std::string cores, float cpu_num) = 0;
};

class IADO_proxy_factory : public Component::IBase {
public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0xfacbb389,0x1665,0x4e5b,0xa1b1,0x3c,0xff,0x4a,0x5e,0xe2,0x63);
  // clang-format on

  virtual IADO_proxy *create(Component::IKVStore::pool_t pool_id,
                             const std::string &filename,
                             std::vector<std::string> &args, std::string cores,
                             int memory, float cpu_num,
                             numa_node_t numa_zone) = 0;
};

#pragma GCC diagnostic pop

} // namespace Component

#endif // __API_ADO_ITF_H__
