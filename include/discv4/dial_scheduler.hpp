// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>

#include <discv4/dial_history.hpp>
#include <discv4/discv4_client.hpp>
#include <rlpx/rlpx_session.hpp>
#include <base/logger.hpp>

namespace discv4 {

/// A discovered peer whose public key has already been validated.
struct ValidatedPeer
{
    DiscoveredPeer   peer;
    rlpx::PublicKey  pubkey;
};

/// @brief Global resource pool shared across all chain DialSchedulers.
///        Enforces a two-level fd cap: total across all chains, and per chain.
///        @p max_total     — global fd cap  (mobile default 12, desktop 200)
///        @p max_per_chain — per-chain cap  (mobile default 3,  desktop 50)
struct WatcherPool
{
    int               max_total;
    int               max_per_chain;
    std::atomic<int>  active_total{0};

    WatcherPool(int max_total_, int max_per_chain_)
        : max_total(max_total_), max_per_chain(max_per_chain_) {}
};

/// Callback signature for what to run per dial attempt.
///   @p vp           — the peer to connect to
///   @p on_done      — call on every exit path (recycling the slot)
///   @p on_connected — call once the ETH handshake is confirmed
///   @p yield        — coroutine yield context
using DialFn = std::function<void(
    ValidatedPeer                                               vp,
    std::function<void()>                                       on_done,
    std::function<void(std::shared_ptr<rlpx::RlpxSession>)>    on_connected,
    boost::asio::yield_context                                  yield)>;

/// @brief Per-chain dial scheduler mirroring go-ethereum's dialScheduler.
///        Maintains up to pool->max_per_chain concurrent dial coroutines,
///        respecting the global pool->max_total cap across all chains.
///        All methods run on the single io_context thread — no mutex needed.
struct DialScheduler : std::enable_shared_from_this<DialScheduler>
{
    boost::asio::io_context&              io;
    std::shared_ptr<WatcherPool>          pool;
    DialFn                                dial_fn;
    std::shared_ptr<DialHistory>          dial_history;

    int                                          active{0};
    int                                          validated_count{0};   ///< currently active validated connections
    int                                          total_validated{0};   ///< cumulative count (never decrements)
    bool                                         stopping{false};
    std::deque<ValidatedPeer>                    queue;
    std::vector<std::weak_ptr<rlpx::RlpxSession>> active_sessions;

    DialScheduler(boost::asio::io_context&  io_,
                  std::shared_ptr<WatcherPool> pool_,
                  DialFn                    dial_fn_)
        : io(io_)
        , pool(std::move(pool_))
        , dial_fn(std::move(dial_fn_))
        , dial_history(std::make_shared<DialHistory>())
    {}

    /// @brief Enqueue a validated peer for dialing.
    ///        If a slot is free (both per-chain and global caps), spawns immediately.
    ///        Otherwise queues for later drain.
    void enqueue(ValidatedPeer vp)
    {
        if (stopping) { return; }

        dial_history->expire();
        if (dial_history->contains(vp.peer.node_id)) { return; }

        if (active < pool->max_per_chain &&
            pool->active_total.load() < pool->max_total)
        {
            ++active;
            ++pool->active_total;
            dial_history->add(vp.peer.node_id);
            spawn_dial(std::move(vp));
        }
        else
        {
            queue.push_back(std::move(vp));
        }
    }

    /// @brief Called by every dial exit path. Recycles the slot and
    ///        drains the queue up to the available capacity.
    void release()
    {
        --active;
        --pool->active_total;

        if (stopping) { return; }

        dial_history->expire();
        while (active < pool->max_per_chain &&
               pool->active_total.load() < pool->max_total &&
               !queue.empty())
        {
            ValidatedPeer vp = std::move(queue.front());
            queue.pop_front();
            if (dial_history->contains(vp.peer.node_id)) { continue; }
            ++active;
            ++pool->active_total;
            dial_history->add(vp.peer.node_id);
            spawn_dial(std::move(vp));
        }
    }

    /// @brief Async stop — disconnect all active sessions immediately.
    ///        Returns immediately; fds are freed on the next io_context cycle.
    void stop()
    {
        stopping = true;
        queue.clear();
        for (auto& ws : active_sessions)
        {
            if (auto s = ws.lock())
            {
                (void)s->disconnect(rlpx::DisconnectReason::kClientQuitting);
            }
        }
        active_sessions.clear();
    }

private:
    void spawn_dial(ValidatedPeer vp)
    {
        auto sched         = shared_from_this();
        auto was_validated = std::make_shared<bool>(false);
        static auto log    = rlp::base::createLogger("dial_scheduler");
        SPDLOG_LOGGER_DEBUG(log, "Dialing peer: {}:{}", vp.peer.ip, vp.peer.tcp_port);
        boost::asio::spawn(io,
            [sched, vp = std::move(vp), was_validated](boost::asio::yield_context yc)
            {
                sched->dial_fn(
                    vp,
                    [sched, was_validated]()
                    {
                        if (*was_validated) { --sched->validated_count; }
                        sched->release();
                    },
                    [sched, was_validated](std::shared_ptr<rlpx::RlpxSession> s)
                    {
                        *was_validated = true;
                        ++sched->validated_count;
                        ++sched->total_validated;
                        sched->active_sessions.push_back(s);
                    },
                    yc);
            });
    }
};

} // namespace discv4
