/* -*- c++ -*- */
/*
 * Copyright 2010 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <vrt/expanded_if_context_section.h>
#include <string.h>
//#include <gruel/inet.h>
#include <arpa/inet.h>
#include <boost/format.hpp>
#include "header_utils.h"

using boost::format;
using boost::io::group;


namespace vrt
{
  using namespace detail;

  void
  expanded_if_context_section::clear()
  {
    context_indicator = 0;
    ref_point_id = 0;
    bandwidth = 0;
    if_ref_freq = 0;
    rf_ref_freq = 0;
    rf_ref_freq_offset = 0;
    if_band_offset = 0;
    ref_level = 0;
    gain = 0;
    over_range_count = 0;
    sample_rate = 0;
    timestamp_adj = 0;
    timestamp_cal_time = 0;
    temperature = 0;
    device_id[0] = 0;
    device_id[1] = 0;
    state_and_event_ind = 0;
    memset(&payload_fmt, 0, sizeof(payload_fmt));
    memset(&formatted_gps, 0, sizeof(formatted_gps));
    memset(&formatted_ins, 0, sizeof(formatted_ins));
    memset(&ecef_ephemeris, 0, sizeof(ecef_ephemeris));
    memset(&rel_ephemeris, 0, sizeof(rel_ephemeris));
    ephemeris_ref_id = 0;
    gps_ascii.manuf_oui = 0;
    gps_ascii.ascii.clear();
    cntx_assoc_lists.source.clear();
    cntx_assoc_lists.system.clear();
    cntx_assoc_lists.vector_comp.clear();
    cntx_assoc_lists.async_channel.clear();
    cntx_assoc_lists.async_tag.clear();
  }

  struct unpack_info {
    const uint32_t 	*p;
    const size_t	nwords;
    size_t		i;

    unpack_info(const uint32_t *p_, size_t nwords_)
      : p(p_), nwords(nwords_), i(0) {}

    bool consumed_all()
    {
      return nwords - i == 0;
    }

    bool ensure(size_t n)
    {
      return (nwords - i) >= n;
    }

    bool get_int32(int32_t &x)
    {
      if (!ensure(1))
	return false;
      x = ntohl(p[i++]);
      return true;
    }

    bool get_uint32(uint32_t &x)
    {
      if (!ensure(1))
	return false;
      x = ntohl(p[i++]);
      return true;
    }

    bool get_hertz(vrt_hertz_t &x)
    {
      if (!ensure(2))
	return false;
      
      uint32_t hi = ntohl(p[i++]);
      uint32_t lo = ntohl(p[i++]);
      x = ((int64_t) hi) << 32 | lo;
      return true;
    }

    bool get_db(vrt_db_t &x)
    {
      if (!ensure(1))
	return false;
      x = ntohl(p[i++]);
      return true;
    }

    bool get_gain(vrt_gain_t &x)
    {
      if (!ensure(1))
	return false;
      x = ntohl(p[i++]);
      return true;
    }

    bool get_int64(int64_t &x)
    {
      if (!ensure(2))
	return false;
      
      uint32_t hi = ntohl(p[i++]);
      uint32_t lo = ntohl(p[i++]);
      x = ((int64_t) hi) << 32 | lo;
      return true;
    }

    bool get_temp(vrt_temp_t &x)
    {
      if (!ensure(1))
	return false;
      x = ntohl(p[i++]) & 0xffff;
      return true;
    }

    bool get_nwords(uint32_t *x, unsigned int nw)
    {
      if (!ensure(nw))
	return false;

      for (unsigned int j = 0; j < nw; j++)
	x[j] = ntohl(p[i++]);

      return true;
    }

    bool get_nwords_vector(std::vector<uint32_t> &x, unsigned int nw)
    {
      if (!ensure(nw))
	  return false;
      x.resize(nw);
      return get_nwords(&x[0], nw);
    }

    bool get_formatted_gps(vrt_formatted_gps_t &x)
    {
      return get_nwords((uint32_t *) &x, 11);
    }

    bool get_ephemeris(vrt_ephemeris_t &x)
    {
      return get_nwords((uint32_t *) &x, 13);
    }

    bool get_gps_ascii(exp_gps_ascii &x)
    {
      uint32_t	manuf_oui;
      uint32_t  nw;

      if (!get_uint32(manuf_oui) || !get_uint32(nw))
	return false;

      if (!ensure(nw))
	return false;

      const char *s = (const char *)&p[i];
      size_t nbytes = strnlen(s, nw * sizeof(uint32_t));
      x.manuf_oui = manuf_oui;
      x.ascii = std::string(s, nbytes);
      i += nw;
      return true;
    }

    bool get_cntx_assoc_lists(exp_context_assocs &x)
    {
      uint32_t	w0;
      uint32_t	w1;

      if (!get_uint32(w0) || !get_uint32(w1))
	return false;

      uint32_t source_list_size = (w0 >> 16) & 0x1ff;
      uint32_t system_list_size = w0 & 0x1ff;
      uint32_t vector_comp_list_size = (w1 >> 16) & 0xffff;
      uint32_t async_channel_list_size =  w1 & 0x7fff;
      bool a_bit = (w1 & 0x8000) != 0;
      uint32_t async_tag_list_size = a_bit ? async_channel_list_size : 0;

      return (true
	      && get_nwords_vector(x.source, source_list_size)
	      && get_nwords_vector(x.system, system_list_size)
	      && get_nwords_vector(x.vector_comp, vector_comp_list_size)
	      && get_nwords_vector(x.async_channel, async_channel_list_size)
	      && get_nwords_vector(x.async_tag, async_tag_list_size));
    }

  };

  bool 
  expanded_if_context_section::unpack(const uint32_t *context_section,	// in
				      size_t n32_bit_words,		// in
				      expanded_if_context_section *e)	// out
  {
    unpack_info u(context_section, n32_bit_words);
    e->clear();

    if (!u.get_uint32(e->context_indicator))
      return false;
    uint32_t cif = e->context_indicator;
    
    if (cif & CI_REF_POINT_ID)
      if (!u.get_uint32(e->ref_point_id))
	return false;

    if (cif & CI_BANDWIDTH)
      if (!u.get_hertz(e->bandwidth))
	return false;

    if (cif & CI_IF_REF_FREQ)
      if (!u.get_hertz(e->if_ref_freq))
	return false;

    if (cif & CI_RF_REF_FREQ)
      if (!u.get_hertz(e->rf_ref_freq))
	return false;

    if (cif & CI_RF_REF_FREQ_OFFSET)
      if (!u.get_hertz(e->rf_ref_freq_offset))
	return false;

    if (cif & CI_IF_BAND_OFFSET)
      if (!u.get_hertz(e->if_band_offset))
	return false;

    if (cif & CI_REF_LEVEL)
      if (!u.get_db(e->ref_level))
	return false;

    if (cif & CI_GAIN)
      if (!u.get_gain(e->gain))
	return false;

    if (cif & CI_OVER_RANGE_COUNT)
      if (!u.get_uint32(e->over_range_count))
	return false;

    if (cif & CI_SAMPLE_RATE)
      if (!u.get_hertz(e->sample_rate))
	return false;

    if (cif & CI_TIMESTAMP_ADJ)
      if (!u.get_int64(e->timestamp_adj))
	return false;

    if (cif & CI_TIMESTAMP_CAL_TIME)
      if (!u.get_uint32(e->timestamp_cal_time))
	return false;

    if (cif & CI_TEMPERATURE)
      if (!u.get_temp(e->temperature))
	return false;

    if (cif & CI_DEVICE_ID)
      if (!u.get_uint32(e->device_id[0]) || !u.get_uint32(e->device_id[1]))
	return false;
    
    if (cif & CI_STATE_AND_EVENT_IND)
      if (!u.get_uint32(e->state_and_event_ind))
	return false;

    if (cif & CI_PAYLOAD_FMT)
      if (!u.get_uint32(e->payload_fmt.word0) || !u.get_uint32(e->payload_fmt.word1))
	return false;

    if (cif & CI_FORMATTED_GPS)
      if (!u.get_formatted_gps(e->formatted_gps))
	return false;

    if (cif & CI_FORMATTED_INS)
      if (!u.get_formatted_gps(e->formatted_ins))
	return false;

    if (cif & CI_ECEF_EPHEMERIS)
      if (!u.get_ephemeris(e->ecef_ephemeris))
	return false;

    if (cif & CI_REL_EPHEMERIS)
      if (!u.get_ephemeris(e->rel_ephemeris))
	return false;

    if (cif & CI_EPHEMERIS_REF_ID)
      if (!u.get_int32(e->ephemeris_ref_id))
	return false;

    if (cif & CI_GPS_ASCII)
      if (!u.get_gps_ascii(e->gps_ascii))
	return false;

    if (cif & CI_CNTX_ASSOC_LISTS)
      if (!u.get_cntx_assoc_lists(e->cntx_assoc_lists))
	return false;

    return u.consumed_all();
  }

  static void
  wr_cntx_list(std::ostream &os, const std::string &name, const std::vector<uint32_t> &v)
  {
    if (v.empty())
      return;

    wr_name(os, "  " + name);
    for (size_t j = 0; j < v.size(); j++)
      os << format("%#x ") % v[j];
    os << std::endl;
  }

  static void
  wr_cntx_assoc_lists(std::ostream &os, const exp_context_assocs &x)
  {
    os << std::endl;
    wr_cntx_list(os, "source", x.source);
    wr_cntx_list(os, "system", x.system);
    wr_cntx_list(os, "vector", x.vector_comp);
    wr_cntx_list(os, "async_chan", x.async_channel);
    wr_cntx_list(os, "async_tag",  x.async_tag);
  }

  void
  expanded_if_context_section::write(std::ostream &os) const
  {
    uint32_t cif = context_indicator;

    if (cif & CI_REF_POINT_ID){
      wr_name(os, "ref_point_id");
      wr_uint32_hex(os, ref_point_id);
    }

    if (cif & CI_BANDWIDTH){
      wr_name(os, "bandwidth");
      wr_hertz(os, bandwidth);
    }

    if (cif & CI_IF_REF_FREQ){
      wr_name(os, "if_ref_freq");
      wr_hertz(os, if_ref_freq);
    }

    if (cif & CI_RF_REF_FREQ){
      wr_name(os, "rf_ref_freq");
      wr_hertz(os, rf_ref_freq);
    }

    if (cif & CI_RF_REF_FREQ_OFFSET){
      wr_name(os, "rf_ref_freq_offset");
      wr_hertz(os, rf_ref_freq_offset);
    }

    if (cif & CI_IF_BAND_OFFSET){
      wr_name(os, "if_band_offset");
      wr_hertz(os, if_band_offset);
    }

    if (cif & CI_REF_LEVEL){
      wr_name(os, "ref_level");
      wr_dbm(os, ref_level);
    }

    if (cif & CI_GAIN){
      wr_name(os, "gain stage1");
      wr_db(os, vrt_gain_stage1(gain));
      wr_name(os, "gain stage2");
      wr_db(os, vrt_gain_stage2(gain));
    }

    if (cif & CI_OVER_RANGE_COUNT){
      wr_name(os, "over_range_count");
      wr_uint32_dec(os, over_range_count);
    }

    if (cif & CI_SAMPLE_RATE){
      wr_name(os, "sample_rate");
      wr_hertz(os, sample_rate);
    }

    if (cif & CI_TIMESTAMP_ADJ){
      wr_name(os, "timestamp_adj");
      os << format("%10d ps\n") % timestamp_adj;
    }

    if (cif & CI_TIMESTAMP_CAL_TIME){
      wr_name(os, "timestamp_cal_time");
      wr_int_secs(os, timestamp_cal_time);
    }

    if (cif & CI_TEMPERATURE){
      wr_name(os, "temperature");
      wr_temp(os, temperature);
    }

    if (cif & CI_DEVICE_ID){
      wr_name(os, "manuf_oui");
      wr_uint32_hex(os, device_id[0] & 0x00ffffff);
      wr_name(os, "device_code");
      wr_uint32_hex(os, device_id[1] & 0xffff);
    }

    if (cif & CI_STATE_AND_EVENT_IND){
      wr_name(os, "state_and_event_ind");
      wr_uint32_hex(os, state_and_event_ind);
    }

    if (cif & CI_PAYLOAD_FMT){
      wr_name(os, "payload_fmt");
      wr_payload_fmt(os, payload_fmt);
    }

    if (cif & CI_FORMATTED_GPS){
      wr_name(os, "formatted_gps");
      wr_formatted_gps(os, formatted_gps);
    }

    if (cif & CI_FORMATTED_INS){
      wr_name(os, "formatted_ins");
      wr_formatted_gps(os, formatted_ins);
    }

    if (cif & CI_ECEF_EPHEMERIS){
      wr_name(os, "ecef_ephemeris");
      os << "<NOT IMPLEMENTED>\n";
      // wr_ephemeris(os, ecef_ephemeris);
    }

    if (cif & CI_REL_EPHEMERIS){
      wr_name(os, "rel_ephemeris");
      os << "<NOT IMPLEMENTED>\n";
      // wr_ephemeris(os, rel_ephemeris);
    }

    if (cif & CI_EPHEMERIS_REF_ID){
      wr_name(os, "epemeris_ref_id");
      wr_uint32_hex(os, ephemeris_ref_id);
    }

    if (cif & CI_GPS_ASCII){
      wr_name(os, "gps_ascii");
      os << "<NOT IMPLEMENTED>\n";
      // wr_gps_ascii(os, gps_ascii);
    }

    if (cif & CI_CNTX_ASSOC_LISTS){
      wr_name(os, "cntx_assoc_lists");
      wr_cntx_assoc_lists(os, cntx_assoc_lists);
    }

  }

  std::ostream& operator<<(std::ostream &os, const expanded_if_context_section &obj)
  {
    obj.write(os);
    return os;
  }

}; // namespace vrt
