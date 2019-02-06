//
//  A:  0: recv[0], 0: send [kBusyMs]
//       0: on_packet_cb_(), next_ready_ = now() + kBusyMs
//       0: PostDelayedTask(send, min(now(), next_ready_))
//
//  B:  0: recv[0], 0: send[kBusyMs], 2: recv[2]
//       0: on_packet_cb_(), next_ready_ = now() + kBusyMs
//       1: PostDelayedTask(send, min(now(), next_ready_))
//       2: on_packet_cb_()
//
//  C:  0: recv, 1: send, 2: send (kBusyMs): recv
//       0: on_packet_cb_(), next_ready_ = now() + kBusyMs
//       1: PostDelayedTask(send, min(now(), next_ready_)) -> Thunk1
//       (kBusyMs): on_packet_cb_(), next_ready_ = now() + kBusyMs
//          [Thunk1] -> PostDelayedTask(send, min(now(), next_ready_ + kBusyMs))
//       [now() + kBusyMs]: sent.
//
//  D:  0: send, 1: send, 2: recv
//       0: PostDelayedTask(send, min(now(), next_ready_), next_ready_ = now() + kBusyMs
//       1: PostDelayedTask(send, min(now(), next_ready_), next_ready_ = now() + kBusyMs
//       2: on_packet_cb_(), next_ready_ = now() + kBusyMs
//
//  D:  0: send, 1: recv
//       0: PostDelayedTask(send, min(now(), next_ready_), next_ready_ = now() + kBusyMs
//       1: on_packet_cb_(), next_ready_ = now() + kBusyMs
//
//  E:  0: send, 1: send, (3,4,5,6,7,8,9,10,11,12,13,14): recv, 
//       0: PostDelayedTask(send, min(now(), next_ready_), next_ready_ = now() + kBusyMs
//       1: Enqueued.
//       3,4,5,6,7,8,9,10,11: on_packet_cb_(), next_ready_ = now() + kBusyMs
//       11: Sent
