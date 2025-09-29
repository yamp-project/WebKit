/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/create_peerconnection_factory.h"

#include <memory>
#include <utility>

#include "api/audio/audio_device.h"
#include "api/audio/audio_mixer.h"
#include "api/audio/audio_processing.h"
#include "api/audio/builtin_audio_processing_builder.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/create_modular_peer_connection_factory.h"
#include "api/enable_media.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials_view.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#if defined(WEBRTC_WEBKIT_BUILD)
#include "api/task_queue/default_task_queue_factory.h"
#endif
#include "api/transport/field_trial_based_config.h"
#include "api/scoped_refptr.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "rtc_base/thread.h"

namespace webrtc {

scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    Thread* network_thread,
    Thread* worker_thread,
    Thread* signaling_thread,
    scoped_refptr<AudioDeviceModule> default_adm,
    scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory,
    scoped_refptr<AudioMixer> audio_mixer,
    scoped_refptr<AudioProcessing> audio_processing,
    std::unique_ptr<AudioFrameProcessor> audio_frame_processor,
    std::unique_ptr<FieldTrialsView> field_trials
#if defined(WEBRTC_WEBKIT_BUILD)
    , std::unique_ptr<TaskQueueFactory> task_queue_factory
#endif
  ) {
  if (!field_trials) {
    field_trials = std::make_unique<webrtc::FieldTrialBasedConfig>();
  }

  PeerConnectionFactoryDependencies dependencies;
  dependencies.network_thread = network_thread;
  dependencies.worker_thread = worker_thread;
  dependencies.signaling_thread = signaling_thread;
  dependencies.event_log_factory = std::make_unique<RtcEventLogFactory>();
  dependencies.env = CreateEnvironment(std::move(field_trials));

  if (network_thread) {
    // TODO(bugs.webrtc.org/13145): Add an SocketFactory* argument.
    dependencies.socket_factory = network_thread->socketserver();
  }
  dependencies.adm = std::move(default_adm);
  dependencies.audio_encoder_factory = std::move(audio_encoder_factory);
  dependencies.audio_decoder_factory = std::move(audio_decoder_factory);
  dependencies.audio_frame_processor = std::move(audio_frame_processor);
  if (audio_processing != nullptr) {
    dependencies.audio_processing_builder =
        CustomAudioProcessing(std::move(audio_processing));
  } else {
#ifndef WEBRTC_EXCLUDE_AUDIO_PROCESSING_MODULE
    dependencies.audio_processing_builder =
        std::make_unique<BuiltinAudioProcessingBuilder>();
#endif
  }
  dependencies.audio_mixer = std::move(audio_mixer);
  dependencies.video_encoder_factory = std::move(video_encoder_factory);
  dependencies.video_decoder_factory = std::move(video_decoder_factory);
  EnableMedia(dependencies);

  return CreateModularPeerConnectionFactory(std::move(dependencies));
}

}  // namespace webrtc
