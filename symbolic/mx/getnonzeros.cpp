/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010 by Joel Andersson, Moritz Diehl, K.U.Leuven. All rights reserved.
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "getnonzeros.hpp"
#include "../stl_vector_tools.hpp"
#include "../matrix/matrix_tools.hpp"
#include "mx_tools.hpp"
#include "../sx/sx_tools.hpp"
#include "../fx/sx_function.hpp"
#include "../matrix/sparsity_tools.hpp"

using namespace std;

namespace CasADi{

  GetNonzeros::GetNonzeros(const CRSSparsity& sp, const MX& y, const std::vector<int>& nz) : NonzerosBase(nz){
    setSparsity(sp);
    setDependencies(y);
  }

  void GetNonzeros::evaluateD(const DMatrixPtrV& input, DMatrixPtrV& output, const DMatrixPtrVV& fwdSeed, DMatrixPtrVV& fwdSens, const DMatrixPtrVV& adjSeed, DMatrixPtrVV& adjSens){
    evaluateGen<double,DMatrixPtrV,DMatrixPtrVV>(input,output,fwdSeed,fwdSens,adjSeed,adjSens);
  }

  void GetNonzeros::evaluateSX(const SXMatrixPtrV& input, SXMatrixPtrV& output, const SXMatrixPtrVV& fwdSeed, SXMatrixPtrVV& fwdSens, const SXMatrixPtrVV& adjSeed, SXMatrixPtrVV& adjSens){
    evaluateGen<SX,SXMatrixPtrV,SXMatrixPtrVV>(input,output,fwdSeed,fwdSens,adjSeed,adjSens);
  }

  template<typename T, typename MatV, typename MatVV>
  void GetNonzeros::evaluateGen(const MatV& input, MatV& output, const MatVV& fwdSeed, MatVV& fwdSens, const MatVV& adjSeed, MatVV& adjSens){

    // Number of sensitivities
    int nadj = adjSeed.size();
    int nfwd = fwdSens.size();
    
    // Nondifferentiated outputs
    const vector<T>& idata = input[0]->data();
    typename vector<T>::iterator odata_it = output[0]->begin();
    for(vector<int>::const_iterator k=nz_.begin(); k!=nz_.end(); ++k){
      *odata_it++ = *k>=0 ? idata[*k] : 0;
    }
    
    // Forward sensitivities
    for(int d=0; d<nfwd; ++d){
      const vector<T>& fseed = fwdSeed[d][0]->data();
      typename vector<T>::iterator fsens_it = fwdSens[d][0]->begin();
      for(vector<int>::const_iterator k=nz_.begin(); k!=nz_.end(); ++k){
     	*fsens_it++ = *k>=0 ? fseed[*k] : 0;
      }
    }
      
    // Adjoint sensitivities
    for(int d=0; d<nadj; ++d){
      typename vector<T>::iterator aseed_it = adjSeed[d][0]->begin();
      vector<T>& asens = adjSens[d][0]->data();
      for(vector<int>::const_iterator k=nz_.begin(); k!=nz_.end(); ++k){
	if(*k>=0) asens[*k] += *aseed_it;
	*aseed_it++ = 0;
      }
    }
  }
  
  void GetNonzeros::propagateSparsity(DMatrixPtrV& input, DMatrixPtrV& output, bool fwd){
    // Get references to the assignment operations and data
    bvec_t *outputd = get_bvec_t(output[0]->data());
    bvec_t *inputd = get_bvec_t(input[0]->data());
    
    // Propate sparsity
    if(fwd){
      for(vector<int>::const_iterator k=nz_.begin(); k!=nz_.end(); ++k){
	*outputd++ = *k>=0 ? inputd[*k] : 0;
      }
    } else {
      for(vector<int>::const_iterator k=nz_.begin(); k!=nz_.end(); ++k){
	if(*k>=0) inputd[*k] |= *outputd;
	*outputd++ = 0;
      }
    }
  }

  void GetNonzeros::printPart(std::ostream &stream, int part) const{
    switch(part){
    case 1: stream << nz_; break;
    }
  }

  void GetNonzeros::evaluateMX(const MXPtrV& input, MXPtrV& output, const MXPtrVV& fwdSeed, MXPtrVV& fwdSens, const MXPtrVV& adjSeed, MXPtrVV& adjSens, bool output_given){

    // Number of derivative directions
    int nfwd = fwdSens.size();
    int nadj = adjSeed.size();

    // Output sparsity
    const CRSSparsity &osp = sparsity();
    const vector<int>& ocol = osp.col();
    vector<int> orow = osp.getRow();
    
    // Input sparsity
    const CRSSparsity &isp = dep().sparsity();
    const vector<int>& icol = isp.col();
    vector<int> irow = isp.getRow();
      
    // We next need to resort the assignment vector by inputs instead of outputs
    // Start by counting the number of input nonzeros corresponding to each output nonzero
    vector<int> inz_count(icol.size()+1,0);
    for(vector<int>::const_iterator it=nz_.begin(); it!=nz_.end(); ++it){
      casadi_assert_message(*it>=0,"Not implemented");
      inz_count[*it+1]++;
    }
    
    // Cumsum to get index offset for input nonzero
    for(int i=0; i<icol.size(); ++i){
      inz_count[i+1] += inz_count[i];
    }
    
    // Get the order of assignments
    vector<int> nz_order(nz_.size());
    for(int k=0; k<nz_.size(); ++k){
      // Save the new index
      nz_order[inz_count[nz_[k]]++] = k;
    }
    
    // Find out which elements are given
    vector<int>& el_input = inz_count; // Reuse memory
    el_input.resize(nz_.size());
    for(int k=0; k<nz_.size(); ++k){
      // Get output nonzero
      int inz_k = nz_[nz_order[k]];
      
      // Get element (note: may contain duplicates)
      el_input[k] = irow[inz_k] + icol[inz_k]*isp.size1();
    }
    
    // Sparsity pattern being formed and corresponding nonzero mapping
    vector<int> r_rowind, r_col, r_nz;
        
    // Nondifferentiated function and forward sensitivities
    int first_d = output_given ? 0 : -1;
    for(int d=first_d; d<nfwd; ++d){

      // Get references to arguments and results
      MX& arg = d<0 ? *input[0] : *fwdSeed[d][0];
      MX& res = d<0 ? *output[0] : *fwdSens[d][0];      
      
      // Get the matching nonzeros
      r_nz.resize(el_input.size());
      copy(el_input.begin(),el_input.end(),r_nz.begin());
      arg.sparsity().getNZInplace(r_nz);
      
      // Add to sparsity pattern
      int n=0;
      r_col.clear();
      r_rowind.resize(osp.size1()+1); // Row count
      fill(r_rowind.begin(),r_rowind.end(),0);
      for(int k=0; k<nz_.size(); ++k){
	if(r_nz[k]!=-1){
	  r_nz[n++] = r_nz[k];
	  r_col.push_back(ocol[nz_order[k]]);
	  r_rowind[1+orow[nz_order[k]]]++;
	}
      }
      r_nz.resize(n);
      for(int i=1; i<r_rowind.size(); ++i) r_rowind[i] += r_rowind[i-1]; // row count -> row offset

      // Create a sparsity pattern from vectors
      CRSSparsity f_sp(osp.size1(),osp.size2(),r_col,r_rowind);
      if(r_nz.size()==0){
	res = MX::zeros(f_sp);
      } else {
	res = arg->getGetNonzeros(f_sp,r_nz);
      }
    }

    // Quick return if no adjoints
    if(nadj==0) return;

    // Get all input elements (this time without duplicates)
    isp.getElements(el_input,false);
    
    // Temporary for sparsity pattern unions
    vector<unsigned char> tmp1;

    // Adjoint sensitivities
    for(int d=0; d<nadj; ++d){

      // Get an owning references to the seeds and sensitivities and clear the seeds for the next run
      MX aseed = *adjSeed[d][0];
      *adjSeed[d][0] = MX();
      MX& asens = *adjSens[d][0]; // Sensitivity after addition
      MX asens0 = asens; // Sensitivity before addition
      
      // Get the corresponding nz locations in the output sparsity pattern
      aseed.sparsity().getElements(r_nz,false);
      osp.getNZInplace(r_nz);

      // Filter out ignored entries and check if there is anything to add at all
      bool elements_to_add = false;
      for(vector<int>::iterator k=r_nz.begin(); k!=r_nz.end(); ++k){
	if(*k>=0){
	  if(nz_[*k]>=0){
	    elements_to_add = true;
	  } else {
	    *k = -1;
	  }
	}
      }

      // Quick continue of no elements to add
      if(!elements_to_add) continue;
     
      // Get the nz locations in the adjoint sensitivity corresponding to the inputs
      vector<int> &r_nz2 = r_col; // Reuse memory
      r_nz2.resize(el_input.size());
      copy(el_input.begin(),el_input.end(),r_nz2.begin());
      asens0.sparsity().getNZInplace(r_nz2);
      
      // Enlarge the sparsity pattern of the sensitivity if not all additions fit
      for(vector<int>::iterator k=r_nz.begin(); k!=r_nz.end(); ++k){
	if(*k>=0 && r_nz2[nz_[*k]]<0){
	  
	  // Create a new pattern which includes both the the previous seed and the addition
	  CRSSparsity sp = asens0.sparsity().patternUnion(dep().sparsity(),tmp1);
	  asens0 = asens0->getDensification(sp);

	  // Recalculate the nz locations in the adjoint sensitivity corresponding to the inputs
	  copy(el_input.begin(),el_input.end(),r_nz2.begin());
	  asens0.sparsity().getNZInplace(r_nz2);

	  break;
	}
      }

      // Have r_nz point to locations in the sensitivity instead of the output
      for(vector<int>::iterator k=r_nz.begin(); k!=r_nz.end(); ++k){
	if(*k>=0){
	  *k = r_nz2[nz_[*k]];
	}
      }

      // Add to the element to the sensitivity
      asens = aseed->getAddNonzeros(asens0,r_nz);
    }
  }
  
  Matrix<int> GetNonzeros::mapping(int iind) const {
    return Matrix<int>(sparsity(),nz_);
  }

  bool GetNonzeros::isIdentity() const{
    // Check sparsity
    if(!(sparsity() == dep().sparsity()))
      return false;
      
    // Check if the nonzeros follow in increasing order
    for(int k=0; k<nz_.size(); ++k){
      if(nz_[k] != k) return false;
    }
    
    // True if reached this point
    return true;
  }

  void GetNonzeros::generateOperation(std::ostream &stream, const std::vector<std::string>& arg, const std::vector<std::string>& res, CodeGenerator& gen) const{
    // Condegen the indices
    int ind = gen.getConstant(nz_,true);
    
    // Codegen the assignments
    stream << "  for(ii=s" << ind << ", rr=" << res.front() << ", ss=" << arg.front() << "; ii!=s" << ind << "+" << nz_.size() << "; ++ii) *rr++ = *ii>=0 ? ss[*ii] : 0;" << endl;
  }

  void GetNonzeros::simplifyMe(MX& ex){
    // Simplify if identity
    if(isIdentity()){
      MX t = dep(0);
      ex = t;
    }
  }

  MX GetNonzeros::getGetNonzeros(const CRSSparsity& sp, const std::vector<int>& nz) const{
    // Eliminate recursive calls
    vector<int> nz_new(nz);
    for(vector<int>::iterator i=nz_new.begin(); i!=nz_new.end(); ++i){
      *i = nz_[*i];
    }
    return dep()->getGetNonzeros(sp,nz_new);
  }

  GetNonzerosSlice::GetNonzerosSlice(const CRSSparsity& sp, const MX& x, const std::vector<int>& nz) : GetNonzeros(sp,x,nz), s_(Slice(nz)){
  }
 
  void GetNonzerosSlice::printPart(std::ostream &stream, int part) const{
    switch(part){
    case 1: stream << "[" << s_ << "]"; break;
    }
  }

    void GetNonzerosSlice::generateOperation(std::ostream &stream, const std::vector<std::string>& arg, const std::vector<std::string>& res, CodeGenerator& gen) const{
      stream << "  for(rr=" << res.front() << ", ss=" << arg.front() << "+" << s_.start_ << "; ss!=" << arg.front() << "+" << s_.stop_ << "; ss+=" << s_.step_ << ") ";
      stream << "*rr++ = *ss;" << endl;
    }

  GetNonzerosSlice2::GetNonzerosSlice2(const CRSSparsity& sp, const MX& x, const std::vector<int>& nz) : GetNonzeros(sp,x,nz){
    inner_ = Slice(nz,outer_);
  }
 
  void GetNonzerosSlice2::printPart(std::ostream &stream, int part) const{
    switch(part){
    case 1: stream << "[" << outer_ << ";" << inner_ << "]"; break;
    }
  }

  void GetNonzerosSlice2::generateOperation(std::ostream &stream, const std::vector<std::string>& arg, const std::vector<std::string>& res, CodeGenerator& gen) const{
    stream << "  for(rr=" << res.front() << ", ss=" << arg.front() << "+" << outer_.start_ << "; ss!=" << arg.front() << "+" << outer_.stop_ << "; ss+=" << outer_.step_ << ") ";
    stream << "for(tt=ss+" << inner_.start_ << "; tt!=ss+" << inner_.stop_ << "; tt+=" << inner_.step_ << ") ";
    stream << "*rr++ = *tt;" << endl;
  }

} // namespace CasADi
