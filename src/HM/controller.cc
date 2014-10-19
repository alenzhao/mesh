#include "classdef.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>
#include <math.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <omp.h>


void eQTL_controller::load_data(char *filename, int csize, int gsize){
  
  // initialization of class parameters

  pi0 = new double[1];
  
  grid_size = gsize;
  // grid prior starts w/ equal weights
  grid_wts = new double[gsize];
  new_grid_wts = new double[gsize];
      
  config_size = csize;
  
  if(config_size>0){
    config_prior = new double[config_size];
    new_config_prior = new double[config_size];
  } 
  
  
  // reading data file
  gene_eQTL geq;
    
  string sid;
  string stype;
  
  string curr_sid;   // current gene-snp 
  string curr_gene;  // current gene
  string curr_snp;
  
  char id_str[128];

  ifstream infile(filename);
  string line;
  char delim[] = "\t";
  char *snp_id;
  
  vector<vector<double> > sbf_vec;


  while(getline(infile,line)){
     
    const char *cline = line.c_str();
    char *c_line = new char[strlen(cline)+1];
    memset(c_line,0,strlen(cline)+1);
    strcpy(c_line,cline);

    char *token;
  
    
    // parse the line
    char *sid = strtok(c_line,delim);
    
    if(strcmp(sid, curr_sid.c_str())!=0){
      
      memset(id_str,0,128);
      memcpy(id_str,sid,strlen(sid));
      char *p = strchr(id_str,'_');
      *p = 0;
      snp_id = id_str;
      char *gene_id = p+1;
      
      // if gene changes
      if(strcmp(gene_id,curr_gene.c_str())!=0){
	
	if(sbf_vec.size()!=config_size && sbf_vec.size()!=0){
	  fprintf(stderr, "Error: %s contains information for %d types of model BFs, expecting %d \n", curr_sid.c_str(),int(sbf_vec.size()),config_size);
	  exit(1);
	}

	if(sbf_vec.size()!=0){
	  // process current recorded gene
	  geq.snpVec.push_back(snp_eQTL(curr_snp,sbf_vec,config_prior,grid_wts));
	  geqVec.push_back(geq);
	}
	
	curr_gene = string(gene_id);
	curr_snp = string(snp_id);
	geq = gene_eQTL(curr_gene,pi0);
	
	// create new gene_eqtl structure
      }else{      
	// if only snp changes
	if(sbf_vec.size()!=0)
	  // create new snp_eqtl structure
	  geq.snpVec.push_back(snp_eQTL(curr_snp,sbf_vec,config_prior,grid_wts));
	
	curr_snp = string(snp_id);
      }
      
      curr_sid = string(sid);
      
      // clean up
      for(int i=0;i<sbf_vec.size();i++)
	sbf_vec[i].clear();
      sbf_vec.clear();
      
    }



    char *mtype = strtok(0,delim);  
    if(type_vec.size() < csize){
      
      string ts(mtype);
      type_vec.push_back(ts);
      //printf("%s\n",ts.c_str());
    }
    
    vector<double> bfv;
    
    while((token=strtok(0, delim))!=0){
      bfv.push_back(atof(token));
    }
    
    if(bfv.size()!= grid_size){
      fprintf(stderr, "Error: Model %s of %s has %d BFs, expecting %d\n",mtype,sid,int(bfv.size()),grid_size);
      exit(1);
    }

    sbf_vec.push_back(bfv);
    
    delete[] c_line; 
  }
  

  // last gene
  if(sbf_vec.size()!=0){  
    
    if(sbf_vec.size()!=config_size){
      fprintf(stderr, "Error: %s contains information for %d types of model BFs, expecting %d \n", curr_sid.c_str(),int(sbf_vec.size()),config_size);
      exit(1);
    }   
    geq.snpVec.push_back(snp_eQTL(curr_snp,sbf_vec,config_prior,grid_wts));
    geqVec.push_back(geq);
  
  }
  
  /*
  for(int i=0;i<csize;i++){
    printf("%s\n",type_vec[i].c_str());
  }
  
  exit(0);

  */
  
}

void eQTL_controller::estimate_profile_ci(){
  
  double tick = 0.001;
  double max = compute_log10_lik();
  
  // estimating for pi0
  double pi0_mle = pi0[0];
  double left_pi0 = pi0[0];
  double right_pi0 = pi0[0];

  while(left_pi0>=0){
    left_pi0 -= tick;
    if(left_pi0<0){
      left_pi0 = 0;
      break;
    }
    pi0[0] = left_pi0;  
    if(compute_log10_lik()/log10(exp(1))<max/log10(exp(1))-2.0){  
      left_pi0 += tick; 
      break;
    }    
  }
  
  while(right_pi0<=1){
    right_pi0 += tick;
    if(right_pi0 >1){
      right_pi0 = 1;
      break;
    }
    pi0[0] = right_pi0;  
    if(compute_log10_lik()/log10(exp(1))< max/log10(exp(1))-2.0){
      right_pi0 -= tick;
      break;
    }
    
  }
  
  fprintf(stderr,"\nProfile-likelihood Confidence Intervals\n");
  fprintf(stderr,"pi0: %.3f [%.3f, %.3f]\n",pi0_mle,left_pi0,right_pi0);
  fprintf(stderr,"config: ");
  
  pi0[0] = pi0_mle;
  
  double *config_mle = new double[config_size];
  memcpy(config_mle, config_prior, config_size*sizeof(double));
  
  for (int i=0;i<config_size;i++){
    double right_cp = config_mle[i];
    double left_cp = config_mle[i];
    double cp_mle = config_mle[i];
    
    double st = 1 - cp_mle;
 

    while(left_cp>=0){
      left_cp -= tick;
      if(left_cp<0){
	left_cp = 0;
	break;
      }
      double diff = cp_mle - left_cp;
      for(int j=0;j<config_size;j++){
	if(j==i)
	  continue;
	config_prior[j] = config_mle[j] + diff*config_mle[j]/st;
      }
      config_prior[i] = left_cp;
      
      if(compute_log10_lik()/log10(exp(1))<max/log10(exp(1))-2.0){
	left_cp += tick;
	break;
      }
    }
    
    while(right_cp<=1){
      right_cp += tick;
      if(right_cp>1){
	right_cp = 1;
	break;
      }
      double diff = cp_mle - right_cp;
      for(int j=0;j<config_size;j++){
        if(j==i)
	  continue;
        config_prior[j] = config_mle[j]+ diff*config_mle[j]/st;
      }
      config_prior[i] = right_cp;
      if(compute_log10_lik()/log10(exp(1))<max/log10(exp(1))-2.0){
        right_cp -= tick;
	break;
      }
    }
    
    fprintf(stderr,"%s  %.3f [%.3f, %.3f]  ",type_vec[i].c_str(), config_mle[i],left_cp, right_cp);
    fflush(stdout);
  }
  
  memcpy(config_prior, config_mle,config_size*sizeof(double));

  fprintf(stderr,"\n");
  delete[] config_mle;
    
  fprintf(stderr,"grid:  ");

  double *grid_mle = new double[grid_size];
  memcpy(grid_mle, grid_wts, grid_size*sizeof(double));

  for (int i=0;i<grid_size;i++){
    double right_cp = grid_mle[i];
    double left_cp = grid_mle[i];
    double cp_mle = grid_mle[i];

    double st = 1 - cp_mle;


    while(left_cp>=0){
      left_cp -= tick;
      if(left_cp<0){
        left_cp = 0;
        break;
      }
      double diff = cp_mle - left_cp;
      for(int j=0;j<grid_size;j++){
        if(j==i)
          continue;
        grid_wts[j] = grid_mle[j] + diff*grid_mle[j]/st;
      }
      grid_wts[i] = left_cp;

      if(compute_log10_lik()/log10(exp(1))<max/log10(exp(1))-2.0){
        left_cp += tick;
        break;
      }
    }

    while(right_cp<=1){
      right_cp += tick;
      if(right_cp>1){
        right_cp = 1;
        break;
      }
      double diff = cp_mle - right_cp;
      for(int j=0;j<grid_size;j++){
        if(j==i)
          continue;
        grid_wts[j] = grid_mle[j]+ diff*grid_mle[j]/st;
      }
      grid_wts[i] = right_cp;
      if(compute_log10_lik()/log10(exp(1))<max/log10(exp(1))-2.0){
        right_cp -= tick;
        break;
	
      }
    }
    
    fprintf(stderr,"%.3f [%.3f, %.3f]  ",grid_mle[i],left_cp, right_cp);
    fflush(stdout);
  }

  fprintf(stderr,"\n");
  delete[] grid_mle;

}



void eQTL_controller::init_params(char *init_file){
  
  ifstream ifile(init_file);
  string line;
  istringstream ins;
  
  while(getline(ifile,line)){

    ins.clear();
    ins.str(line);
    double value;
    string type;
    ins>>type;
    
    if(strcmp(type.c_str(),"pi0")==0||strcmp(type.c_str(),"Pi0")==0){
      ins>>value;
      pi0[0] = value;
      continue;
    }
    
    if(strcmp(type.c_str(),"config")==0){
      for(int i=0;i<config_size;i++){
	ins>>value;
	config_prior[i] = value;
      }
      continue;
    }
    
    if(strcmp(type.c_str(),"grid")==0){
      for(int i=0;i<grid_size;i++){
        ins>>value;
	grid_wts[i] = value;
      }
      continue;
    }
    
  }

  
#pragma omp parallel for num_threads(nthread)
  for(int i=0;i<geqVec.size();i++){
    geqVec[i].set_snp_prior();
  }
  //printf("%f %f %f\n",pi0[0],config_prior[0],grid_wts[0]);
  
  ifile.close();
}


 

void eQTL_controller::init_params(int option){
  #pragma omp parallel for num_threads(nthread)
  for(int i=0;i<geqVec.size();i++){
    geqVec[i].set_snp_prior();
  }

  if(option==1){
    randomize_parameter_sp();
    return;
  }
  
  pi0[0] = 0.5;
  
  if(config_size == 1){
    config_prior[0] = 1.0;
    new_config_prior[0] = 1.0;
  }else{
    for(int i=0;i<config_size;i++){
      config_prior[i] = 1.0/(config_size);
    }
    
  }
  
  for(int i=0;i<grid_size;i++){
    grid_wts[i] = 1.0/grid_size;
  }
  

}

 
void eQTL_controller::randomize_parameter_sp(){
  
  const gsl_rng_type * T;
  gsl_rng * r;
       
  gsl_rng_env_setup();
  T = gsl_rng_default;
  r = gsl_rng_alloc (T);

  long seed = time (NULL);    
  gsl_rng_set (r, seed); 
  
  
  //double x1 = gsl_ran_exponential(r,1.0);
  //double x2 = gsl_ran_exponential(r,1.0);
  //pi0[0] = .90 + .1*x1/(x1 + x2);
  //pi0[0] = x1/(x1 + x2);
  pi0[0] = gsl_rng_uniform (r);
  
  
  double sum = 0;
  for(int i=0;i<grid_size;i++){
    grid_wts[i] = gsl_ran_exponential(r,1.0);
    sum += grid_wts[i];
  }

  for(int i=0;i<grid_size;i++){
    grid_wts[i] /= sum;
  }

  sum = 0;
  if(config_size==1){
    new_config_prior[0]=config_prior[0] = 1.0;
  }else{
    for(int i=0;i<config_size;i++){
      config_prior[i] = gsl_ran_exponential(r,1.0);
      sum+= config_prior[i];
    }

    for(int i=0;i<config_size;i++){
      config_prior[i] /= sum;
    }
  }
  
  gsl_rng_free (r);

  return;

}



double eQTL_controller::compute_log10_lik(){
  
  double sum = 0;

#pragma omp parallel for num_threads(nthread) reduction(+:sum)
  for (int i=0;i<geqVec.size();i++){
    sum += geqVec[i].compute_log10_lik();
  }
  
  return sum;
}


void eQTL_controller::em_update_snp_prior(){
  
#pragma omp parallel for num_threads(nthread)
  for (int i=0;i<geqVec.size();i++){
     geqVec[i].em_update_snp_prior();
  }

}


void eQTL_controller::em_update_pi0(){
  
  // update pi0
  double sum = 0;

#pragma omp parallel for num_threads(nthread) reduction(+:sum)
  for(int i=0;i<geqVec.size();i++){
    //geqVec[i].update_snp_prior();
    sum += geqVec[i].em_update_pi0();
  }
  
  new_pi0 = sum/geqVec.size();
  return;
  
}


void eQTL_controller::em_update_grid(){

  double **matrix = new double *[grid_size];
  double *gene_wts = new double[geqVec.size()];
  for(int i=0;i<grid_size;i++){
    matrix[i] = new double[geqVec.size()];
    memset(matrix[i],0,sizeof(double)*geqVec.size());
  }


  double *new_grid = new double[grid_size];
  memset(new_grid,0,sizeof(double)*grid_size);

  
#pragma omp parallel for num_threads(nthread) shared(matrix)
  for(int i=0;i<geqVec.size();i++){
    gene_wts[i] = 1.0;
    double *rst = geqVec[i].em_update_grid(grid_size);
    for(int j=0;j<grid_size;j++)
      matrix[j][i] = rst[j];
    delete[] rst;
  }

  for(int i=0;i<grid_size;i++){
    new_grid[i] = log10_weighted_sum(matrix[i],gene_wts,geqVec.size())+log10(grid_wts[i]);
    delete[] matrix[i];
  }

  delete[] matrix;
  delete[] gene_wts;

  double *wts = new double[grid_size];
  for(int i=0;i<grid_size;i++){
    wts[i] = 1.0;
  }

  double sum = log10_weighted_sum(new_grid,wts,grid_size);
  delete[] wts;

  for(int i=0;i<grid_size;i++)
    new_grid_wts[i] = pow(10, (new_grid[i] - sum));

  delete[] new_grid;

  return;


}



void eQTL_controller::em_update_config(){
  
  
  if(config_size==1)
    return;
  
  double **matrix = new double *[config_size];
  double *gene_wts = new double[geqVec.size()];
  for(int i=0;i<config_size;i++){
    matrix[i] = new double[geqVec.size()];
    memset(matrix[i],0,sizeof(double)*geqVec.size());
  }

  
  double *new_config = new double[config_size];
  memset(new_config,0,sizeof(double)*config_size);
  
#pragma omp parallel for num_threads(nthread) shared(matrix)
  for(int i=0;i<geqVec.size();i++){
    gene_wts[i] = 1.0;   
    double *rst = geqVec[i].em_update_config(config_size);
    for(int j=0;j<config_size;j++)
      matrix[j][i] = rst[j];
    delete[] rst;
  }
  
  for(int i=0;i<config_size;i++){
    new_config[i] = log10_weighted_sum(matrix[i],gene_wts,geqVec.size())+log10(config_prior[i]);
    delete[] matrix[i];
  }
  
  delete[] matrix;
  delete[] gene_wts;
  
  double *wts = new double[config_size];
  for(int i=0;i<config_size;i++){
    wts[i] = 1.0;
  }

  double sum = log10_weighted_sum(new_config,wts,config_size);
  delete[] wts;
    
  for(int i=0;i<config_size;i++)
    new_config_prior[i] = pow(10, (new_config[i] - sum));
  
  delete[] new_config;
  
  return;


}

void eQTL_controller::update_params(){
 
  
  *pi0 = new_pi0;
  if(config_size>1){
    for(int i=0;i<config_size;i++)
      config_prior[i] = new_config_prior[i];
  }
  
  for(int i=0;i<grid_size;i++)
    grid_wts[i] = new_grid_wts[i];
 
  
  //for (int i=0;i<geqVec.size();i++){
  //  geqVec[i].update_snp_prior();
  //}
  

}


void eQTL_controller::run_EM(double thresh){  

  // start iteration
  
  int count = 1;
  double last_log10_lik = -9999999;
  //new_pi0 = *pi0 = 0.50;
  while(1){
    
    double curr_log10_lik = compute_log10_lik();
    em_update_pi0();    
    em_update_config();
    em_update_grid();
   
    //em_update_snp_prior();
      

    update_params();
    
    if(output_option==1){
      // output 
      fprintf(stderr,"iter  %4d  loglik = %.4f   ",count++,curr_log10_lik);
      fprintf(stderr,"pi0  %.4f     config ", pi0[0]);
	      
      for(int i=0;i<config_size;i++)
	fprintf(stderr,"%.3f  ",new_config_prior[i]);

      fprintf(stderr,"  grid ");

      for(int i=0;i<grid_size;i++)
      	fprintf(stderr,"%.3f  ",new_grid_wts[i]);

      fprintf(stderr,"\n");
    }
    
    if(fabs(curr_log10_lik-last_log10_lik)<thresh)
      break;
    
    last_log10_lik = curr_log10_lik;
  }

}

void eQTL_controller::compute_posterior(){
  
#pragma omp parallel for num_threads(nthread)
  for(int i=0;i<geqVec.size();i++){
    geqVec[i].compute_posterior(config_prior, config_size);
  }
}
    



void eQTL_controller::print_result(){
  printf("gene_name\tgene_posterior_prob\tgene_log10_bf\t\tsnp_name\tsnp_log10_bf\t\t");
  for(int i=0;i<type_vec.size();i++){
    printf("%s\t",type_vec[i].c_str());
  }
  printf("\n");
  
  for(int i=0;i<geqVec.size();i++)
    geqVec[i].print_result(type_vec);
}

