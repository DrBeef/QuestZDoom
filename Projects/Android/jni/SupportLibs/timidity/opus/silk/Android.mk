LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := silk

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
$(LOCAL_PATH)/float/ \
$(LOCAL_PATH)/../ \
$(LOCAL_PATH)/../celt/ \
$(LOCAL_PATH)/../include \
$(LOCAL_PATH)/../../libogg/include

# Deer god.
LOCAL_SRC_FILES := CNG.c code_signs.c init_decoder.c decode_core.c decode_parameters.c \
	               decode_frame.c NLSF_unpack.c LPC_inv_pred_gain.c \
				   decode_indices.c decode_pulses.c decoder_set_fs.c dec_API.c enc_API.c \
		   		   encode_indices.c encode_pulses.c gain_quant.c interpolate.c LP_variable_cutoff.c	\
				   NLSF_decode.c NSQ.c NSQ_del_dec.c PLC.c shell_coder.c tables_gain.c \
				   tables_LTP.c tables_NLSF_CB_NB_MB.c tables_NLSF_CB_WB.c tables_other.c \
				   tables_pitch_lag.c tables_pulses_per_block.c VAD.c control_audio_bandwidth.c \
				   quant_LTP_gains.c VQ_WMat_EC.c HP_variable_cutoff.c NLSF_encode.c NLSF_VQ.c \
				   NLSF_del_dec_quant.c process_NLSFs.c stereo_LR_to_MS.c stereo_MS_to_LR.c \
				   check_control_input.c control_SNR.c init_encoder.c control_codec.c A2NLSF.c \
				   ana_filt_bank_1.c biquad_alt.c bwexpander_32.c bwexpander.c debug.c \
				   decode_pitch.c inner_prod_aligned.c lin2log.c log2lin.c LPC_analysis_filter.c \
				   table_LSF_cos.c NLSF2A.c NLSF_stabilize.c NLSF_VQ_weights_laroia.c \
				   pitch_est_tables.c resampler.c resampler_down2_3.c resampler_down2.c \
				   resampler_private_AR2.c resampler_private_down_FIR.c \
				   resampler_private_IIR_FIR.c resampler_private_up2_HQ.c resampler_rom.c \
				   sigm_Q15.c sort.c sum_sqr_shift.c stereo_decode_pred.c stereo_encode_pred.c \
				   stereo_find_predictor.c stereo_quant_pred.c \
				   float/apply_sine_window_FLP.c float/corrMatrix_FLP.c float/encode_frame_FLP.c \
				   float/find_LPC_FLP.c float/find_LTP_FLP.c float/find_pitch_lags_FLP.c \
				   float/find_pred_coefs_FLP.c float/LPC_analysis_filter_FLP.c \
				   float/LTP_analysis_filter_FLP.c float/LTP_scale_ctrl_FLP.c \
				   float/noise_shape_analysis_FLP.c float/prefilter_FLP.c float/process_gains_FLP.c \
				   float/regularize_correlations_FLP.c float/residual_energy_FLP.c float/solve_LS_FLP.c\
				   float/warped_autocorrelation_FLP.c float/wrappers_FLP.c float/autocorrelation_FLP.c \
				   float/burg_modified_FLP.c float/bwexpander_FLP.c float/energy_FLP.c \
				   float/inner_product_FLP.c float/k2a_FLP.c float/levinsondurbin_FLP.c \
				   float/LPC_inv_pred_gain_FLP.c float/pitch_analysis_core_FLP.c \
				   float/scale_copy_vector_FLP.c float/scale_vector_FLP.c float/schur_FLP.c \
				   float/sort_FLP.c
# TODO: Add Arch things.

#LOCAL_SHARED_LIBRARIES := liblpc10 libgsm libogg	
#LOCAL_STATIC_LIBRARIES := vorbisfile vorbisenc
LOCAL_CFLAGS           := -Wall -DHAVE_CONFIG_H
#LOCAL_LDFLAGS          := -Wl,-Map,xxx.map
#LOCAL_LDLIBS := -ldl -lGLESv1_CM -llog -L$(DIRECTORY_TO_OBJ)

include $(BUILD_STATIC_LIBRARY)



