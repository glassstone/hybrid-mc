#ifndef _BINNER_H__
#define _BINNER_H__

#include <iostream>
#include <iomanip>
#include <fstream>
#include <assert.h>
#include <math.h>
#include <limits>
#include <vector>
#include <omp.h>
#include <boost/shared_ptr.hpp>

#include "ndarray.h"
#include <string.h>


/** **************************************************************************************************************************************************************************
 * ND Binner
 *****************************************************************************************************************************************************************************/

// TODO: Write accessors and marginalizers
class TBinnerND {
	// Data
	NDArray<double> *bin;
	double *min, *max, *dx;
	unsigned int N;
	
	unsigned int *index_workspace;
	
public:
	// Constructor & Destructor
	TBinnerND(double *_min, double *_max, unsigned int *width, unsigned int _N);
	~TBinnerND();
	
	// Mutators //////////////////////////////////////////////////////////////////////////////////////////////
	void add_point(double *pos, double weight);	// Add data point to binner
	void clear();					// Set the bins to zero
	void normalize(bool to_peak=true);		// Normalize the bins either to the peak value, or to sum to unity
	
	void operator ()(double *pos, double weight) { add_point(pos, weight); }
	
	// Accessors /////////////////////////////////////////////////////////////////////////////////////////////
	void write_to_file(std::string fname, bool ascii=true, bool log_pdf=true);	// Write the binned data to a binary file
	void print_bins();								// Print out the bins to cout
};

TBinnerND::TBinnerND(double *_min, double *_max, unsigned int *width, unsigned int _N)
	: bin(NULL), min(NULL), max(NULL), dx(NULL), N(_N), index_workspace(NULL)
{
	min = new double[N];
	max = new double[N];
	dx = new double[N];
	index_workspace = new unsigned int[N];
	for(unsigned int i=0; i<N; i++) {
		min[i] = _min[i];
		max[i] = _max[i];
		dx[i] = (max[i]-min[i])/(double)width[i];
	}
	
	bin = new NDArray<double>(width, N);
	clear();
}

TBinnerND::~TBinnerND()
{
	delete[] min;
	delete[] max;
	delete[] dx;
	delete[] index_workspace;
	delete bin;
}

// Set the bins to zero
void TBinnerND::clear() {
	bin->fill(0.);
}

// Add data point to binner
void TBinnerND::add_point(double *pos, double weight) {
	// Check bounds
	for(unsigned int i=0; i<N; i++) {
		if((pos[i] < min[i]) || (pos[i] > max[i])) { return; }
	}
	// Add point
	for(unsigned int i=0; i<N; i++) { index_workspace[i] = (unsigned int)((pos[i] - min[i]) / dx[i]); }
	#pragma omp atomic
	bin->get_element(index_workspace, N) += weight;
}

// Normalize the bins either to the peak value, or to sum to unity
void TBinnerND::normalize(bool to_peak) {
	double norm = 0.;
	unsigned int N_bins = bin->get_size();
	if(to_peak) {	// Normalize to the peak probability
		for(unsigned int i=0; i<N_bins; i++) { if(bin->get_element(i) > norm) { norm = bin->get_element(i); } }
	} else {	// Normalize total probability to unity
		for(unsigned int i=0; i<N_bins; i++) { norm += bin->get_element(i); }
	}
	for(unsigned int i=0; i<N_bins; i++) { bin->get_element(i) /= norm; }
}

// Write the binned data to a binary file
void TBinnerND::write_to_file(std::string fname, bool ascii, bool log_pdf) {
	double log_bin_min = std::numeric_limits<double>::infinity();
	double tmp_bin;
	unsigned int *pos = new unsigned int[N];
	if(log_pdf) {
		NDArray<double>::iterator i_end = bin->end();
		for(NDArray<double>::iterator i=bin->begin(); i != i_end; ++i) {
			tmp_bin = *i;
			if((tmp_bin != 0.) && (tmp_bin < log_bin_min)) { log_bin_min = tmp_bin; }
		}
		log_bin_min = log(log_bin_min);
	}
	if(ascii) {
		std::ofstream outfile(fname.c_str());
		NDArray<double>::iterator i_end = bin->end();
		for(NDArray<double>::iterator i=bin->begin(); i != i_end; ++i) {
			tmp_bin = *i;
			if(log_pdf && (tmp_bin == 0.)) { tmp_bin = log_bin_min - 2.; } else { tmp_bin = log(tmp_bin); }
			i.get_pos(pos);
			for(unsigned int n=0; n<N; n++) {
				outfile << min[0]+dx[0]*((double)pos[n]+0.5) << "\t";
			}
			outfile << tmp_bin << "\n";
		}
		outfile.close();
	} else {
		std::fstream outfile(fname.c_str(), std::ios::binary | std::ios::out);
		for(unsigned int i=0; i<N; i++) {					// This second section gives the dimensions of the mesh
			unsigned int width_i = bin->get_width(i);
			outfile.write(reinterpret_cast<char *>(&N), sizeof(unsigned int));
			outfile.write(reinterpret_cast<char *>(&width_i), sizeof(unsigned int));
			outfile.write(reinterpret_cast<char *>(&min[i]), sizeof(double));
			outfile.write(reinterpret_cast<char *>(&max[i]), sizeof(double));
			outfile.write(reinterpret_cast<char *>(&dx[i]), sizeof(double));
		}
		NDArray<double>::iterator i_end = bin->end();
		for(NDArray<double>::iterator i=bin->begin(); i != i_end; ++i) {
			tmp_bin = *i;
			if(log_pdf && (tmp_bin == 0.)) { tmp_bin = log_bin_min - 2.; } else { tmp_bin = log(tmp_bin); }
			outfile.write(reinterpret_cast<char *>(&tmp_bin), sizeof(double));
		}
		outfile.close();
	}
	delete[] pos;
}

/** **************************************************************************************************************************************************************************
 * 2D Binner
 *****************************************************************************************************************************************************************************/

// Fast and simple 2D data binner
template<unsigned int N>
struct TBinner2D {
	// Data
	double min[2];
	double max[2];
	double dx[2];
	unsigned int width[2];
	unsigned int bin_dim[2];
	double **bin;
	
	// Constructor & Destructor
	TBinner2D(double (&_min)[2], double (&_max)[2], unsigned int (&_width)[2], unsigned int (&_bin_dim)[2]);
	~TBinner2D();
	
	// Mutators //////////////////////////////////////////////////////////////////////////////////////////////
	void add_point(double (&pos)[N], double weight);	// Add data point to binner
	void clear();						// Set the bins to zero
	void normalize(bool to_peak=true);			// Normalize the bins either to the peak value, or to sum to unity
	
	void operator ()(double (&pos)[N], double weight) { add_point(pos, weight); }
	
	// Accessors /////////////////////////////////////////////////////////////////////////////////////////////
	void write_to_file(std::string fname, bool ascii=true, bool log_pdf=true);	// Write the binned data to a binary file
	void print_bins();								// Print out the bins to cout
};


template<unsigned int N>
TBinner2D<N>::TBinner2D(double (&_min)[2], double (&_max)[2], unsigned int (&_width)[2], unsigned int (&_bin_dim)[2]) {
	for(unsigned int i=0; i<2; i++) {
		min[i] = _min[i];
		max[i] = _max[i];
		width[i] = _width[i];
		assert(_bin_dim[i] < N);
		bin_dim[i] = _bin_dim[i];
		dx[i] = (max[i]-min[i])/(double)width[i];
	}
	bin = new double*[width[0]];
	for(unsigned int i=0; i<width[0]; i++) {
		bin[i] = new double[width[1]];
		//for(unsigned int k=0; k<width[1]; k++) { bin[i][k] = 0.; }
	}
	clear();
}

template<unsigned int N>
TBinner2D<N>::~TBinner2D() {
	for(unsigned int i=0; i<width[0]; i++) { delete[] bin[i]; }
	delete[] bin;
}

// Set the bins to zero
template<unsigned int N>
void TBinner2D<N>::clear() {
	for(unsigned int i=0; i<width[0]; i++) {
		for(unsigned int k=0; k<width[1]; k++) { bin[i][k] = 0.; }
	}
}

// Add data point to binner
template<unsigned int N>
void TBinner2D<N>::add_point(double (&pos)[N], double weight) {
	// Check bounds
	for(unsigned int i=0; i<2; i++) {
		if((pos[bin_dim[i]] < min[i]) || (pos[bin_dim[i]] > max[i])) { return; }
	}
	// Add point
	unsigned int index[2];
	for(unsigned int i=0; i<2; i++) { index[i] = (unsigned int)((pos[bin_dim[i]] - min[i]) / dx[i]); }
	#pragma omp atomic
	bin[index[0]][index[1]] += weight;
}

// Normalize the bins either to the peak value, or to sum to unity
template<unsigned int N>
void TBinner2D<N>::normalize(bool to_peak) {
	double norm = 0.;
	if(to_peak) {	// Normalize to the peak probability
		for(unsigned int j=0; j<width[0]; j++) {
			for(unsigned int k=0; k<width[1]; k++) { if(bin[j][k] > norm) { norm = bin[j][k]; } }
		}
	} else {	// Normalize total probability to unity
		for(unsigned int j=0; j<width[0]; j++) {
			for(unsigned int k=0; k<width[1]; k++) { norm += bin[j][k]; }
		}
	}
	for(unsigned int j=0; j<width[0]; j++) {
		for(unsigned int k=0; k<width[1]; k++) { bin[j][k] /= norm; }
	}
}

// Write the binned data to a binary file
template<unsigned int N>
void TBinner2D<N>::write_to_file(std::string fname, bool ascii, bool log_pdf) {
	double log_bin_min = std::numeric_limits<double>::infinity();
	double tmp_bin;
	if(log_pdf) {
		for(unsigned int j=0; j<width[0]; j++) {
			for(unsigned int k=0; k<width[1]; k++) { if((bin[j][k] != 0.) && (bin[j][k] < log_bin_min)) { log_bin_min = bin[j][k]; } }
		}
		log_bin_min = log(log_bin_min);
	}
	if(ascii) {
		std::ofstream outfile(fname.c_str());
		for(unsigned int j=0; j<width[0]; j++) {
			for(unsigned int k=0; k<width[1]; k++) {
				if(!log_pdf) { tmp_bin = bin[j][k]; } else if(bin[j][k] == 0.) { tmp_bin = log_bin_min - 2.; } else { tmp_bin = log(bin[j][k]); }
				outfile << min[0]+dx[0]*((double)j+0.5) << "\t" << min[1]+dx[1]*((double)k+0.5) << "\t" << tmp_bin << "\n";
			}
		}
		outfile.close();
	} else {
		std::fstream outfile(fname.c_str(), std::ios::binary | std::ios::out);
		unsigned int tmp;
		for(unsigned int i=0; i<2; i++) {					// This second section gives the dimensions of the mesh
			tmp = width[i];
			outfile.write(reinterpret_cast<char *>(&tmp), sizeof(unsigned int));
			outfile.write(reinterpret_cast<char *>(&min[i]), sizeof(double));
			outfile.write(reinterpret_cast<char *>(&max[i]), sizeof(double));
			outfile.write(reinterpret_cast<char *>(&dx[i]), sizeof(double));
		}
		for(unsigned int j=0; j<width[0]; j++) {
			for(unsigned int k=0; k<width[1]; k++) {
				if(!log_pdf) { tmp_bin = bin[j][k]; } else if(bin[j][k] == 0.) { tmp_bin = log_bin_min - 2.; } else { tmp_bin = log(bin[j][k]); }
				outfile.write(reinterpret_cast<char *>(&tmp_bin), sizeof(double));
			}
		}
		outfile.close();
	}
}

template<unsigned int N>
void TBinner2D<N>::print_bins() {
	for(int k=(int)width[1]-1; k>=0; k--) {
		std::cout << std::setprecision(3) << min[1]+dx[1]*((double)k+0.5) << "\t||\t";
		for(unsigned int j=0; j<width[0]; j++) { std::cout << std::setprecision(3) << bin[j][k] << "\t"; }
		std::cout << std::endl;
	}
	for(unsigned int j=0; j<width[0]+2; j++) { std::cout << "====\t"; }
	std::cout << std::endl << "\t||\t";
	for(unsigned int j=0; j<width[0]; j++) { std::cout << std::setprecision(3) << min[0]+dx[0]*((double)j+0.5) << "\t"; }
	std::cout << std::endl;
}


/** **************************************************************************************************************************************************************************
 * Container for multiple 2D Binners
 *****************************************************************************************************************************************************************************/

template<unsigned int N>
class TMultiBinner {
	std::vector< boost::shared_ptr<TBinner2D<N> > > binner_arr;
	
public:
	TMultiBinner() {}
	~TMultiBinner() {}
	
	void operator ()(double (&pos)[N], double weight) {
		for(typename std::vector< boost::shared_ptr<TBinner2D<N> > >::iterator it = binner_arr.begin(); it != binner_arr.end(); ++it) { (**it)(pos, weight); }
	}
	
	void clear() {
		for(typename std::vector< boost::shared_ptr<TBinner2D<N> > >::iterator it = binner_arr.begin(); it != binner_arr.end(); ++it) { (*it)->clear(); }
	}
	
	void add_binner(boost::shared_ptr<TBinner2D<N> > binner) { binner_arr.push_back(binner); }
	
	// WARNING: Pass only TBinner2D<N> pointers that have been created with the <new> command. Otherwise, a segfault occurs when the TMultiBinner<N> object is destroyed.
	void add_binner(TBinner2D<N> *binner) {
		boost::shared_ptr<TBinner2D<N> > binner_ptr(binner);
		binner_arr.push_back(binner_ptr);
	}
	
	TBinner2D<N> *get_binner(unsigned int i) {
		assert(i<binner_arr.size());
		return &(*binner_arr.at(i));
	}
	
	unsigned int get_num_binners() { return binner_arr.size(); }
};

#endif
