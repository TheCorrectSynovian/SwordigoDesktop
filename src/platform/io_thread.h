#pragma once
// ============================================================
//  SwordigoDesktop — Async IO Thread
//  Moves all file reads/writes off the main (GL/emulator) thread.
//  Uses a lock-free work queue — the emulator posts a job and
//  continues running; a dedicated IO thread processes it.
//
//  Usage:
//    io_thread_start();                    — call once at boot
//    io_thread_post_save(path, data, len); — fire-and-forget write
//    io_thread_post_load(path, cb);        — async read, cb called
//                                            on completion (main thread)
//    io_thread_stop();                     — clean shutdown
//
//  Completion callbacks are delivered via io_thread_poll(),
//  which should be called once per frame on the main thread.
// ============================================================

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

// Callback types
using IOLoadCallback = std::function<void(bool ok, std::vector<uint8_t> data)>;

// Start the background IO thread (call once at startup)
void io_thread_start();

// Stop and join the IO thread (call on shutdown)
void io_thread_stop();

// Post a fire-and-forget file write
void io_thread_post_save(const std::string& path,
                         std::vector<uint8_t> data);

// Post an async file read; callback is queued for main-thread dispatch
void io_thread_post_load(const std::string& path,
                         IOLoadCallback callback);

// Dispatch completed load callbacks — call ONCE PER FRAME on main thread
void io_thread_poll();
