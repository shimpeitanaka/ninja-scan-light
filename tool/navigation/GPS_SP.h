/**
 * @file GPS single positioning solver
 *
 */

/*
 * Copyright (c) 2016, M.Naruoka (fenrir)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the naruoka.org nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __GPS_SP_H__
#define __GPS_SP_H__

#include <utility>
#include <vector>
#include <exception>

#include <cmath>

#include "param/matrix.h"
#include "GPS.h"

template <class FloatT>
class GPS_SinglePositioning {
  private:
    GPS_SinglePositioning<FloatT> &operator=(const GPS_SinglePositioning<FloatT> &);
  public:
    typedef FloatT float_t;
    typedef Matrix<float_t> matrix_t;

    typedef GPS_SpaceNode<float_t> space_node_t;
    typedef typename space_node_t::gps_time_t gps_time_t;
    typedef typename space_node_t::Satellite satellite_t;

    typedef typename space_node_t::xyz_t xyz_t;
    typedef typename space_node_t::llh_t llh_t;
    typedef typename space_node_t::enu_t enu_t;

    typedef std::vector<std::pair<int, float_t> > prn_obs_t;

  protected:
    const space_node_t &_space_node;

  public:
    GPS_SinglePositioning(const space_node_t &sn)
        : _space_node(sn) {}

    ~GPS_SinglePositioning(){}

    const space_node_t &space_node() const {return _space_node;}

  protected:
    struct geometric_matrices_t {
      matrix_t G; ///< Design matrix, whose component order corresponds to sat_range.iterator().
      matrix_t W; ///< Weight matrix, whose component order corresponds to sat_range.iterator().
      matrix_t delta_r; ///< Residual matrix, whose component order corresponds to sat_range.iterator().
      geometric_matrices_t(const unsigned int &size)
          : G(size, 4), W(size, size), delta_r(size, 1) {
        for(unsigned int i(0); i < size; ++i){
          G(i, 3) = 1;
        }
      }

      matrix_t least_square() const {
        matrix_t Gt_W(G.transpose() * W);
        return (Gt_W * G).inverse() * Gt_W * delta_r;
      }

      void copy_G_W_row(const geometric_matrices_t &another, const unsigned int &row){
        for(unsigned int j(0); j < 4; ++j){
          G(row, j) = another.G(row, j);
        }
        W(row, row) = another.W(row, row);
      }
    };

  public:
    struct residual_t {
      float_t &residual;
      float_t &los_neg_x;
      float_t &los_neg_y;
      float_t &los_neg_z;
      float_t &weight;
    };

    struct pos_t {
      xyz_t xyz;
      llh_t llh;
    };

    /**
     * Get range residual in accordance with current status
     *
     * @param sat satellite
     * @param range pseudo-range
     * @param time_arrival time when signal arrive at receiver
     * @param usr_pos (temporal solution of) user position in meters
     * @param usr_pos_llh (temporal solution of) user position in latitude, longitude, and altitude format
     * @param receiver_error (temporal solution of) receiver clock error in meters
     * @param mat matrices to be stored, already initialized with appropriate size
     * @param is_coarse_mode if true, precise correction will be skipped.
     * @return (float_t) pseudo range, which includes delay, and exclude receiver/satellite error.
     */
    float_t range_residual(
        const satellite_t &sat,
        const float_t &range,
        const gps_time_t &time_arrival,
        const pos_t &usr_pos,
        const float_t &receiver_error,
        residual_t &residual,
        const bool &is_coarse_mode = false) const {

      // Temporal geometry range
      float_t pseudo_range(range - receiver_error);

      // Clock error correction
      pseudo_range += sat.clock_error(time_arrival, pseudo_range) * space_node_t::light_speed;

      // Calculate satellite position
      xyz_t sat_pos(sat.position(time_arrival, pseudo_range));
      float_t geometric_range(usr_pos.xyz.dist(sat_pos));

      // Calculate residual
      residual.residual = pseudo_range - geometric_range;

      // Setup design matrix
      residual.los_neg_x = -(sat_pos.x() - usr_pos.xyz.x()) / geometric_range;
      residual.los_neg_y = -(sat_pos.y() - usr_pos.xyz.y()) / geometric_range;
      residual.los_neg_z = -(sat_pos.z() - usr_pos.xyz.z()) / geometric_range;

      if(is_coarse_mode){

        residual.weight = 1;
      }else{ // Perform more correction

        enu_t relative_pos(enu_t::relative(sat_pos, usr_pos.xyz));

        // Ionospheric
        residual.residual += _space_node.iono_correction(relative_pos, usr_pos.llh, time_arrival);

        // Tropospheric
        residual.residual += _space_node.tropo_correction(relative_pos, usr_pos.llh);

        // Setup weight
        if(residual.residual > 30.0){
          // If residual is too big, exclude it by decreasing its weight.
          residual.weight = 1E-8;
        }else{
          // elevation weight based on "GPS���p�v���O���~���O"
          residual.weight = std::pow(sin(relative_pos.elevation())/0.8, 2);
          if(residual.weight < 1E-3){residual.weight = 1E-3;}
        }
      }

      return pseudo_range;
    }

    struct user_pvt_t {
      enum {
        ERROR_NO = 0,
        ERROR_UNSOLVED,
        ERROR_IONO_PARAMS_INVALID,
        ERROR_INSUFFICIENT_SATELLITES,
        ERROR_POSITION_LS,
        ERROR_POSITION_NOT_CONVERGED,
        ERROR_DOP,
        ERROR_VELOCITY_LS,
      } error_code;
      gps_time_t receiver_time;
      pos_t user_position;
      float_t receiver_error;
      enu_t user_velocity_enu;
      float_t receiver_error_rate;
      float_t gdop, pdop, hdop, vdop, tdop;

      user_pvt_t()
          : error_code(ERROR_UNSOLVED),
            receiver_time(),
            user_position(), receiver_error(0),
            user_velocity_enu(), receiver_error_rate(0) {}
    };

    /**
     * Calculate User position/velocity with hint
     *
     * @param prn_range PRN, pseudo-range list
     * @param prn_rate PRN, pseudo-range rate list
     * @param receiver_time receiver time at measurement
     * @param user_position_init initial solution of user position in XYZ meters and LLH
     * @param receiver_error_init initial solution of receiver clock error in meters
     * @param good_init if true, initial position and clock error are goodly guessed.
     * @param with_velocity if true, perform velocity estimation.
     * @return calculation results and matrices used for calculation
     * @see update_ephemeris(), register_ephemeris
     */
    user_pvt_t solve_user_pvt(
        const prn_obs_t &prn_range,
        const prn_obs_t &prn_rate,
        const gps_time_t &receiver_time,
        const pos_t &user_position_init,
        const float_t &receiver_error_init,
        const bool &good_init = true,
        const bool &with_velocity = true) const {

      user_pvt_t res;
      res.receiver_time = receiver_time;

      if(!_space_node.is_valid_iono_utc()){
        res.error_code = user_pvt_t::ERROR_IONO_PARAMS_INVALID;
        return res;
      }

      typedef std::vector<std::pair<
          typename space_node_t::satellites_t::const_iterator, float_t> > sat_obs_t;
      sat_obs_t available_sat_range;

      const typename space_node_t::satellites_t &sats(_space_node.satellites());
      for(typename prn_obs_t::const_iterator it(prn_range.begin());
          it != prn_range.end();
          ++it){

        int prn(it->first);
        const typename space_node_t::satellites_t::const_iterator it_sat(sats.find(prn));
        if(it_sat == sats.end()){continue;}
        if(!it_sat->second.ephemeris().is_valid(receiver_time)){continue;}

        // Select satellite only when its ephemeris is available and valid.
        available_sat_range.push_back(typename sat_obs_t::value_type(it_sat, it->second));
      }

      if(available_sat_range.size() < 4){
        res.error_code = user_pvt_t::ERROR_INSUFFICIENT_SATELLITES;
        return res;
      }

      res.user_position = user_position_init;
      res.receiver_error = receiver_error_init;

      gps_time_t time_arrival(
          receiver_time - (res.receiver_error / space_node_t::light_speed));

      geometric_matrices_t geomat(available_sat_range.size());
      sat_obs_t available_sat_pseudorange;

      // Position calculation
      // If initialization is not appropriate, more iteration will be performed.
      bool converged(false);
      for(int i(good_init ? 0 : -2); i < 10; i++){

        available_sat_pseudorange.clear();
        unsigned j(0);
        for(typename sat_obs_t::const_iterator it(available_sat_range.begin());
            it != available_sat_range.end();
            ++it, ++j){

          residual_t residual = {
            geomat.delta_r(j, 0),
            geomat.G(j, 0), geomat.G(j, 1), geomat.G(j, 2),
            geomat.W(j, j)
          };

          const satellite_t &sat(it->first->second);
          float_t range(it->second);

          range = range_residual(sat, range, time_arrival,
              res.user_position, res.receiver_error,
              residual,
              i <= 0);

          if(i <= 0){continue;}
          available_sat_pseudorange.push_back(typename sat_obs_t::value_type(it->first, range));
        }

        if(false){ // debug
          for(typename sat_obs_t::const_iterator it(available_sat_range.begin());
              it != available_sat_range.end();
              ++it){
            std::cerr << "PRN:" << it->first->first << " => "
                << it->second
                << " @ Ephemeris: t_oc => "
                << it->first->second.ephemeris().WN << "w "
                << it->first->second.ephemeris().t_oc << " +/- "
                << (it->first->second.ephemeris().fit_interval / 2) << std::endl;
          }
          std::cerr << "G:" << geomat.G << std::endl;
          std::cerr << "W:" << geomat.W << std::endl;
          std::cerr << "delta_r:" << geomat.delta_r << std::endl;
        }

        try{
          // Least square
          matrix_t delta_x(geomat.least_square());

          xyz_t delta_user_position(delta_x.partial(3, 1, 0, 0));
          res.user_position.xyz += delta_user_position;
          res.user_position.llh = res.user_position.xyz.llh();

          float_t delta_receiver_error(delta_x(3, 0));
          res.receiver_error += delta_receiver_error;
          time_arrival -= (delta_receiver_error / space_node_t::light_speed);

          if(delta_user_position.dist() <= 1E-6){
            converged = true;
            break;
          }
        }catch(std::exception &e){
          res.error_code = user_pvt_t::ERROR_POSITION_LS;
          return res;
        }
      }

      if(!converged){
        res.error_code = user_pvt_t::ERROR_POSITION_NOT_CONVERGED;
        return res;
      }

      try{
        matrix_t C((geomat.G.transpose() * geomat.G).inverse());

        // Calculate DOP
        res.gdop = std::sqrt(C.trace());
        res.pdop = std::sqrt(C.partial(3, 3, 0, 0).trace());
        res.hdop = std::sqrt(C.partial(2, 2, 0, 0).trace());
        res.vdop = std::sqrt(C(2, 2));
        res.tdop = std::sqrt(C(3, 3));
      }catch(std::exception &e){
        res.error_code = user_pvt_t::ERROR_DOP;
        return res;
      }

      if((!prn_range.empty()) && with_velocity){ // Calculate velocity
        typedef std::vector<std::pair<int, int> > index_table_t;
        index_table_t index_table;

        // check correspondence between range and rate.
        int i(0);
        for(typename sat_obs_t::const_iterator it(available_sat_pseudorange.begin());
            it != available_sat_pseudorange.end();
            ++it, ++i){
          int j(0);
          for(typename prn_obs_t::const_iterator it2(prn_rate.begin());
              it2 != prn_rate.end();
              ++it2, ++j){
            if(it->first->first == it2->first){
              index_table.push_back(index_table_t::value_type(i, j));
              break;
            }
          }
        }

        i = 0;
        geometric_matrices_t geomat2(index_table.size());
        for(typename index_table_t::const_iterator it(index_table.begin());
            it != index_table.end();
            ++it, ++i){

          int i_range(it->first), i_rate(it->second);

          const satellite_t &sat(available_sat_pseudorange[i_range].first->second);
          float_t pseudo_range(available_sat_pseudorange[i_range].second);

          // Calculate satellite velocity
          xyz_t sat_vel(sat.velocity(time_arrival, pseudo_range));

          // copy design matrix
          geomat2.copy_G_W_row(geomat, i);

          // Update range rate by subtracting LOS satellite velocity with design matrix G, and clock rate error
          geomat2.delta_r(i, 0) = prn_rate[i_rate].second
              + geomat2.G(i, 0) * sat_vel.x()
              + geomat2.G(i, 1) * sat_vel.y()
              + geomat2.G(i, 2) * sat_vel.z()
              + (sat.clock_error_dot(time_arrival, pseudo_range) * space_node_t::light_speed);
        }

        try{
          // Least square
          matrix_t sol(geomat2.least_square());

          res.user_velocity_enu = enu_t::relative_rel(
              xyz_t(sol.partial(3, 1, 0, 0)), res.user_position.llh);
          res.receiver_error_rate = sol(3, 0);
        }catch(std::exception &e){
          res.error_code = user_pvt_t::ERROR_VELOCITY_LS;
          return res;
        }
      }

      res.error_code = user_pvt_t::ERROR_NO;
      return res;
    }

    /**
     * Calculate User position/velocity with hint
     *
     * @param prn_range PRN, pseudo-range list
     * @param prn_rate PRN, pseudo-range rate list
     * @param receiver_time receiver time at measurement
     * @param user_position_init_xyz initial solution of user position in meters
     * @param receiver_error_init initial solution of receiver clock error in meters
     * @param good_init if true, initial position and clock error are goodly guessed.
     * @param with_velocity if true, perform velocity estimation.
     * @return calculation results and matrices used for calculation
     * @see update_ephemeris(), register_ephemeris
     */
    user_pvt_t solve_user_pvt(
        const prn_obs_t &prn_range,
        const prn_obs_t &prn_rate,
        const gps_time_t &receiver_time,
        const xyz_t &user_position_init_xyz,
        const float_t &receiver_error_init,
        const bool &good_init = true,
        const bool &with_velocity = true) const {
      pos_t user_position_init = {user_position_init_xyz, user_position_init_xyz.llh()};
      return solve_user_pvt(
          prn_range, prn_rate, receiver_time,
          user_position_init, receiver_error_init,
          good_init, with_velocity);
    }

    /**
     * Calculate User position/velocity without hint
     *
     * @param prn_range PRN, pseudo-range list
     * @param prn_rate PRN, pseudo-range rate list
     * @param receiver_time receiver time at measurement
     * @return calculation results and matrices used for calculation
     */
    user_pvt_t solve_user_pvt(
        const prn_obs_t &prn_range,
        const prn_obs_t &prn_rate,
        const gps_time_t &receiver_time) const {
      return solve_user_pvt(prn_range, prn_rate, receiver_time, xyz_t(), 0, false);
    }

    /**
     * Calculate User position with hint
     *
     * @param prn_range PRN, pseudo-range list
     * @param receiver_time receiver time at measurement
     * @param user_position_init initial solution of user position in meters
     * @param receiver_error_init initial solution of receiver clock error in meters
     * @param good_init if true, initial position and clock error are goodly guessed.
     * @return calculation results and matrices used for calculation
     */
    user_pvt_t solve_user_position(
        const prn_obs_t &prn_range,
        const gps_time_t &receiver_time,
        const xyz_t &user_position_init,
        const float_t &receiver_error_init,
        const bool &good_init = true) const {

      return solve_user_pvt(
          prn_range, prn_obs_t(), receiver_time,
          user_position_init, receiver_error_init,
          good_init, false);
    }

    /**
     * Calculate User position without hint
     *
     * @param prn_range PRN and pseudo range
     * @param receiver_time receiver time at measurement
     * @return calculation results and matrices used for calculation
     */
    user_pvt_t solve_user_position(
            const prn_obs_t &prn_range,
            const gps_time_t &receiver_time) const {
      return solve_user_position(prn_range, receiver_time, xyz_t(), 0, false);
    }
};

#endif /* __GPS_SP_H__ */