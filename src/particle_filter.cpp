/*
 * particle_filter.cpp
 *
 *  Created on: Mar 31, 2018
 *      Author: Mohamed Ameen
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h>
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

#define NUMBER_OF_PARTICLES 12 // Can be decreased (even 12 particles can pass the test)
#define EPS 0.001  // Just a really small number

using namespace std;
static default_random_engine gen;

void ParticleFilter::init(double x, double y, double theta, double std[])
{
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of
	//   x, y, theta and their uncertainties from GPS) and all weights to 1.
	// Add random Gaussian noise to each particle.

	num_particles = NUMBER_OF_PARTICLES;

	// Create normal distributions for x, y and theta.
	normal_distribution<double> dist_x(x, std[0]);
	normal_distribution<double> dist_y(y, std[1]);
	normal_distribution<double> dist_theta(theta, std[2]);

	particles.resize(num_particles); // Resize the `particles` vector to fit desired number of particles
	weights.resize(num_particles);	 // Resize the `weights` vector to fit desired number of particles

	double init_weight = 1.0/num_particles;
	for (int i = 0; i < num_particles; i++)
	{
		particles[i].id = i;
		particles[i].x = dist_x(gen);
		particles[i].y = dist_y(gen);
		particles[i].theta = dist_theta(gen);
		particles[i].weight = init_weight;
	}
	is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate)
{
	// TODO: Add measurements to each particle and add random Gaussian noise.

	normal_distribution<double> dist_x(0.0, std_pos[0]);
	normal_distribution<double> dist_y(0.0, std_pos[1]);
	normal_distribution<double> dist_theta(0.0, std_pos[2]);

	const double vel_d_t = velocity * delta_t;
	const double yaw_d_t = yaw_rate * delta_t;
	const double vel_yaw = velocity / yaw_rate;

	for (int i = 0; i < num_particles; i++)
	{
		if (fabs(yaw_rate) < EPS)
		{
			particles[i].x += vel_d_t * cos(particles[i].theta);
	    particles[i].y += vel_d_t * sin(particles[i].theta);
    }
    else
		{
			const double theta_new 	= particles[i].theta + yaw_d_t;
	    particles[i].x 					+= vel_yaw * (sin(theta_new) 					- sin(particles[i].theta));
	    particles[i].y 					+= vel_yaw * (cos(particles[i].theta) - cos(theta_new));
	    particles[i].theta 			= theta_new;
		}
	  // Adding random Gaussian noise
	  particles[i].x 			+= dist_x(gen);
	  particles[i].y 			+= dist_y(gen);
	  particles[i].theta 	+= dist_theta(gen);
	}
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations)
{
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the
	//   observed measurement to this particular landmark.

	for (unsigned int i = 0; i < observations.size(); i++)
	{
		// Grab current observation
		LandmarkObs current_opservation = observations[i];

		// Init minimum distance to maximum possible
		double min_dist = numeric_limits<double>::max();

		// Init ID of landmark from map placeholder to be associated with the observation
		int map_id = -1;

		for (unsigned int j = 0; j < predicted.size(); j++)
		{
			// Grab current prediction
	    LandmarkObs current_prediction = predicted[j];

			// Calculate distance between current/predicted landmarks
			double cur_dist = dist(current_opservation.x, current_opservation.y, current_prediction.x, current_prediction.y);

			// Find the predicted landmark nearest the current observed landmark
			if (cur_dist < min_dist)
			{
				min_dist = cur_dist;
				map_id = current_prediction.id;
			}
		}
		// Set the observation's ID to the nearest predicted landmark's ID
		observations[i].id = map_id;
	}
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
		const std::vector<LandmarkObs> &observations, const Map &map_landmarks)
{
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution.
	for (int i = 0; i < num_particles; i++)
	{
		// Get the particle x, y coordinates
    double p_x			= particles[i].x;
    double p_y			= particles[i].y;
    double p_theta	= particles[i].theta;

    // Create a vector to hold the map landmark locations predicted to be within sensor range of the particle
    vector<LandmarkObs> predictions;

    for (unsigned int j = 0; j < map_landmarks.landmark_list.size(); j++)
		{
			// Get ID and x,y coordinates
      float lm_x	= map_landmarks.landmark_list[j].x_f;
      float lm_y	= map_landmarks.landmark_list[j].y_f;
      int lm_id 	= map_landmarks.landmark_list[j].id_i;

      // Only consider landmarks within sensor range of the particle (rather than using the "dist" method considering a circular
      // region around the particle (this considers a rectangular region but is computationally faster)
      if (fabs(lm_x - p_x) <= sensor_range && fabs(lm_y - p_y) <= sensor_range)
			{
        // Add prediction to vector
        predictions.push_back(LandmarkObs{lm_id, lm_x, lm_y });
      }
    }

    // Create and populate a copy of the list of observations transformed from vehicle coordinates to map coordinates
    vector<LandmarkObs> transformed_os;
    for (unsigned int j = 0; j < observations.size(); j++)
		{
      double t_x = cos(p_theta)*observations[j].x - sin(p_theta)*observations[j].y + p_x;
      double t_y = sin(p_theta)*observations[j].x + cos(p_theta)*observations[j].y + p_y;
      transformed_os.push_back(LandmarkObs{observations[j].id, t_x, t_y });
    }

    // Perform dataAssociation for the predictions and transformed observations on current particle
    dataAssociation(predictions, transformed_os);

    // Reinit weight
    particles[i].weight = 1.0;
    for (unsigned int j = 0; j < transformed_os.size(); j++)
		{
			// Placeholders for observation and associated prediction coordinates
      double o_x, o_y, pr_x, pr_y;
      o_x = transformed_os[j].x;
      o_y = transformed_os[j].y;
      int associated_prediction = transformed_os[j].id;
      // Get the x,y coordinates of the prediction associated with the current observation
      for (unsigned int k = 0; k < predictions.size(); k++)
			{
        if (predictions[k].id == associated_prediction)
				{
          pr_x = predictions[k].x;
          pr_y = predictions[k].y;
        }
      }

      // Calculate weight for this observation with multivariate Gaussian
      double s_x 		= std_landmark[0];
      double s_y 		= std_landmark[1];
      double obs_w 	= (1/(2*M_PI*s_x*s_y)) * exp(-( pow(pr_x-o_x,2)/(2*pow(s_x, 2)) + (pow(pr_y-o_y,2)/(2*pow(s_y, 2)))));

      // Product of this obersvation weight with total observations weight
      particles[i].weight *= obs_w;
    }
  }
}

void ParticleFilter::resample()
{
	// TODO: Resample particles with replacement with probability proportional to their weight.
	vector<Particle> new_particles;

  // Get all of the current weights
  vector<double> weights;
  for (int i = 0; i < num_particles; i++)
	{
    weights.push_back(particles[i].weight);
  }

  // Generate random starting index for resampling wheel
  uniform_int_distribution<int> uniintdist(0, num_particles-1);
  auto index = uniintdist(gen);

  // Get max weight
  double max_weight = *max_element(weights.begin(), weights.end());

  // Uniform random distribution [0.0, max_weight)
  uniform_real_distribution<double> unirealdist(0.0, max_weight);

  double beta = 0.0;

  // Spin the resample wheel
  for (int i = 0; i < num_particles; i++)
	{
    beta += unirealdist(gen) * 2.0;
    while (beta > weights[index])
		{
      beta -= weights[index];
      index = (index + 1) % num_particles;
    }
    new_particles.push_back(particles[index]);
  }

  particles = new_particles;
}

Particle ParticleFilter::SetAssociations(Particle& particle, const std::vector<int>& associations,
                                     const std::vector<double>& sense_x, const std::vector<double>& sense_y)
{
    //particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
    // associations: The landmark id that goes along with each listed association
    // sense_x: the associations x mapping already converted to world coordinates
    // sense_y: the associations y mapping already converted to world coordinates

    particle.associations= associations;
    particle.sense_x = sense_x;
    particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}

string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}

string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
