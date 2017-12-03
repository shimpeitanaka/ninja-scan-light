/*
 * Copyright (c) 2015, M.Naruoka (fenrir)
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

#ifndef __MATRIX_H
#define __MATRIX_H

/** @file
 * @brief Portable matrix library
 *
 * This is hand-made matrix library whose features are
 * 1) to use template for generic primitive type
 * including not only double for general purpose,
 * but also int used with fixed float for embedded environment.
 * 2) to utilize shallow copy for small memory usage,
 * which is very important for embedded environment.
 * 3) to use views for transpose and partial matrices
 * to reduce copies.
 *
 * Currently it only supports dense matrices,
 * whose storage is prepared as continuous array,
 * however it can support sparse matrices by extending
 * the Array2D class structure.
 */

#include <string>
#include <exception>

/**
 * @brief Exception related to matrix
 *
 * This exception will be thrown when matrix operation is incorrect.
 */
class MatrixException: public std::exception{
  private:
    std::string what_str;
  public:
    /**
     * Constructor
     *
     * @param what_arg error
     */
    MatrixException(const std::string &what_arg) : what_str(what_arg){}

    ~MatrixException() throw(){}

    const char *what() const throw(){
      return what_str.c_str();
    }
};

/**
 * @brief Exception related to storage
 *
 * This exception will be thrown when access to storage for matrix elements is invalid.
 *
 */
class StorageException: public MatrixException {
  public:
    StorageException(const std::string &what_arg)
        : MatrixException(what_arg) {}
    ~StorageException() throw(){}
};

#if defined(DEBUG)
#define throw_when_debug(e) throw(e)
#else
#define throw_when_debug(e) throw()
#endif

#include <cstring>
#include <cmath>
#include <ostream>
#include "param/complex.h"

/**
 * @brief 2D array abstract class
 *
 * This class provides basic interface of 2D array, such as row and column numbers,
 * accessor for element.
 *
 * @param T precision, for example, double
 */
template<class T>
class Array2D{
  public:
    typedef Array2D<T> self_t;
    typedef Array2D<T> root_t;

  protected:
    unsigned int m_rows;    ///< Rows
    unsigned int m_columns; ///< Columns
    
  public:
    typedef T content_t;

    /**
     * Constructor of Array2D
     *
     * @param rows Rows
     * @param columns Columns
     */
    Array2D(const unsigned int &rows, const unsigned int &columns)
        : m_rows(rows), m_columns(columns){}

    /**
     * Destructor of Array2D
     */
    virtual ~Array2D(){}

    /**
     * Return rows
     *
     * @return (unsigned int) Rows
     */
    const unsigned int &rows() const{return m_rows;}
    /**
     * Return columns
     *
     * @return (int) Columns
     */
    const unsigned int &columns() const{return m_columns;}

    /**
     * Accessor for element
     *
     * @param row Row index (the first row is zero)
     * @param column Column index (the first column is zero)
     * @return (T) 成分
     */
    virtual const T &operator()(
        const unsigned int &row,
        const unsigned int &column) const = 0;
    T &operator()(
        const unsigned int &row,
        const unsigned int &column) {
      return const_cast<T &>(const_cast<const self_t &>(*this)(row, column));
    }
    
    /**
     * Perform zero clear
     *
     */
    virtual void clear() = 0;

    /**
     * Perform copy
     *
     * @param is_deep If true, return deep copy, otherwise return shallow copy (just link).
     * @return root_t Copy
     */
    virtual root_t *copy(const bool &is_deep = false) const = 0;
};

/**
 * @brief Array2D whose elements are dense, and are stored in sequential 1D array.
 * In other words, (i, j) element is mapped to [i * rows + j].
 *
 * @param T precision, for example, double
 */
template <class T>
class Array2D_Dense : public Array2D<T> {
  public:
    typedef Array2D_Dense<T> self_t;
    typedef Array2D<T> super_t;
    typedef Array2D<T> root_t;
    
    using root_t::rows;
    using root_t::columns;

  protected:
    T *values; ///< array for values
    int *ref;  ///< reference counter

    template <class T2>
    static void copy_raw(Array2D_Dense<T2> &dist, const T2 *src){
      std::memcpy(dist.values, src, sizeof(T2) * dist.rows() * dist.columns());
    }

    template <class T2>
    static void clear_raw(Array2D_Dense<T2> &target){
      std::memset(target.values, 0, sizeof(T2) * target.rows() * target.columns());
    }

  public:
    /**
     * Constructor
     *
     * @param rows Rows
     * @param columns Columns
     */
    Array2D_Dense(
        const unsigned int &rows,
        const unsigned int &columns)
        : super_t(rows, columns),
        values(new T[rows * columns]), ref(new int(1)) {
    }
    /**
     * Constructor with initializer
     *
     * @param rows Rows
     * @param columns Columns
     * @param serialized Initializer
     */
    Array2D_Dense(
        const unsigned int &rows,
        const unsigned int &columns,
        const T *serialized)
        : super_t(rows, columns),
        values(new T[rows * columns]), ref(new int(1)) {
      copy_raw(*this, serialized);
    }
    /**
     * Copy constructor, which performs shallow copy.
     *
     * @param array another one
     */
    Array2D_Dense(const self_t &array)
        : super_t(array.m_rows, array.m_columns){
      if(values = array.values){(*(ref = array.ref))++;}
    }
    /**
     * Constructor based on another type array, which performs deep copy.
     *
     * @param array another one
     */
    template <class T2>
    Array2D_Dense(const Array2D<T2> &array)
        : values(new T[array.rows() * array.columns()]), ref(new int(1)) {
      T *buf;
      for(unsigned int i(0); i < array.rows(); ++i){
        for(unsigned int j(0); j < array.rows(); ++j){
          *(buf++) = array(i, j);
        }
      }
    }
    /**
     * Destructor
     *
     * The reference counter will be decreased, and when the counter equals to zero,
     * allocated memory for elements will be deleted.
     */
    ~Array2D_Dense(){
      if(ref && ((--(*ref)) <= 0)){
        delete [] values;
        delete ref;
      }
    }

    /**
     * Assigner, which performs shallow copy.
     *
     * @param array another one
     * @return self_t
     */
    self_t &operator=(const self_t &array){
      if(this != &array){
        if(ref && ((--(*ref)) <= 0)){delete ref; delete [] values;}
        if(values = array.values){
          super_t::m_rows = array.m_rows;
          super_t::m_columns = array.m_columns;
          (*(ref = array.ref))++;
        }
      }
      return *this;
    }

    /**
     * Accessor for element
     *
     * @param row Row index
     * @param column Column Index
     * @return (const T &) Element
     * @throw StorageException It will be thrown when the indices are incorrect.
     */
    const T &operator()(
        const unsigned int &row,
        const unsigned int &column) const throw_when_debug(StorageException) {
#if defined(DEBUG)
      if((row >= rows()) || (column >= columns())){
        throw StorageException("Index incorrect");
      }
#endif
      return values[(row * columns()) + column];
    }

    void clear(){
      clear_raw(*this);
    }

    /**
     * Perform copy
     *
     * @aparm is_deep If true, return deep copy, otherwise return shallow copy (just link).
     * @return (root_t) copy
     */
    root_t *copy(const bool &is_deep = false) const {
      return is_deep ? new self_t(rows(), columns(), values) : new self_t(*this);
    }
};

/**
 * @brief View for specific matrix, such as transpose and partial matrices.
 */
template <bool is_transposed, bool is_partialized>
struct MatrixView {
  static const bool transposed = is_transposed;
  static const bool partialized = is_partialized;

  template <bool is_transposed2, bool is_partialized2, class U = void>
  struct Converter {
    typedef MatrixView<!is_transposed2, is_partialized2> transposed_t;
    typedef MatrixView<is_transposed2, true> partial_t;
  };
  template <class U>
  struct Converter<true, false, U> {
    typedef void transposed_t;
    typedef MatrixView<true, true> partial_t;
  };

  typedef typename Converter<
      is_transposed, is_partialized>::transposed_t transposed_t;
  typedef typename Converter<
      is_transposed, is_partialized>::partial_t partial_t;

  struct {
    unsigned int rows, row_offset;
    unsigned int columns, column_offset;
  } partial;

  MatrixView() {}

  template <bool is_transposed2, bool is_partialized2>
  MatrixView(const MatrixView<is_transposed2, is_partialized2> &another){
    if(another.partialized && partialized){
      partial.rows = another.partial.rows;
      partial.columns = another.partial.columns;
      partial.row_offset = another.partial.row_offset;
      partial.column_offset = another.partial.column_offset;
    }
  }

  static bool is_viewless() {
    return !(transposed || partialized);
  }
};

/**
 * @brief Matrix
 *
 * Most of useful matrix operations are defined.
 *
 * Special care when you want to make copy;
 * The copy constructor(s) and change functions of view such as
 * transpose() are implemented by using shallow copy, which means
 * these return values are linked to their original operand.
 * If you unlink the relation between the original and returned matrices,
 * you have to use copy(), which makes a deep copy explicitly,
 * for example, mat.transpose().copy().
 *
 * @param T precision such as double
 * @param Array2D_Type Storage type. The default is Array2D_Dense
 * @param ViewType View type. The default is void, which means no view, i.e. direct access.
 */
template <
    class T,
    template <class> class Array2D_Type = Array2D_Dense,
    class ViewType = void>
class Matrix{
  public:
    typedef Array2D_Type<T> storage_t;
    typedef Matrix<T, Array2D_Type, ViewType> self_t;

    template <class ViewType2, class U = void>
    struct ViewProperty {
      static const bool transposed = ViewType2::transposed;
      static const bool partialized = ViewType2::partialized;
    };
    template <class U>
    struct ViewProperty<void, U> {
      static const bool transposed = false;
      static const bool partialized = false;
    };

    typedef MatrixView<
        ViewProperty<ViewType>::transposed,
        ViewProperty<ViewType>::partialized> view_t;

    typedef Matrix<T, Array2D_Type, void> viewless_t;
    typedef Matrix<T, Array2D_Type, typename view_t::transposed_t> transposed_t;
    typedef Matrix<T, Array2D_Type, typename view_t::partial_t> partial_t;

    friend viewless_t;
    friend transposed_t;
    friend partial_t;
  protected:
    Array2D<T> *storage; ///< 内部的に利用する2次元配列のメモリ
    view_t view;

    /**
     * Constructor with storage
     *
     * @param storage new storage
     */
    Matrix(Array2D<T> *new_storage) : storage(new_storage), view() {}
    
    inline const storage_t *array2d() const{
      return static_cast<const storage_t *>(storage);
    }
    inline storage_t *array2d() {
      return const_cast<storage_t *>(const_cast<const self_t *>(this)->array2d());
    }

  public:
    /**
     * Return row number.
     *
     * @return row number.
     */
    const unsigned int &rows() const{
      return view.partialized
          ? (view.transposed ? view.partial.columns : view.partial.rows)
          : (view.transposed ? storage->columns() : storage->rows());
    }

    /**
     * Return column number.
     *
     * @return column number.
     */
    const unsigned int &columns() const{
      return view.partialized
          ? (view.transposed ? view.partial.rows : view.partial.columns)
          : (view.transposed ? storage->rows() : storage->columns());
    }

    /**
     * Return matrix element of specified indices.
     *
     * @param row Row index starting from 0.
     * @param column Column index starting from 0.
     * @return element
     */
    const T &operator()(
        const unsigned int &row,
        const unsigned int &column) const {
      return array2d()->storage_t::operator()(
          (view.partialized
              ? (view.transposed
                  ? (column + view.partial.column_offset)
                  : (row + view.partial.row_offset))
              : (view.transposed ? column : row)),
          (view.partialized
              ? (view.transposed
                  ? (row + view.partial.row_offset)
                  : (column + view.partial.column_offset))
              : (view.transposed ? row : column)));
    }
    T &operator()(
        const unsigned int &row,
        const unsigned int &column){
      return const_cast<T &>(const_cast<const self_t &>(*this)(row, column));
    }

    /**
     * Clear elements.
     *
     */
    void clear(){
      if(view.partialized){
        for(unsigned int i(0); i < rows(); i++){
          for(unsigned int j(0); j < columns(); j++){
            (*this)(i, j) = T(0);
          }
        }
      }else{
        array2d()->storage_t::clear();
      }
    }

    /**
     * Constructor without storage.
     *
     */
    Matrix() : storage(NULL), view(){}

    /**
     * Constructor with specified row and column numbers.
     * The storage will be assigned with the size.
     * The elements will be cleared with T(0).
     *
     * @param rows Row number
     * @param columns Column number
     */
    Matrix(
        const unsigned int &rows,
        const unsigned int &columns)
        : storage(new storage_t(rows, columns)), view(){
      clear();
    }

    /**
     * Constructor with specified row and column numbers, and values.
     * The storage will be assigned with the size.
     * The elements will be initialized with specified values。
     *
     * @param rows Row number
     * @param columns Column number
     * @param serialized Initial values of elements
     */
    Matrix(
        const unsigned int &rows,
        const unsigned int &columns,
        const T *serialized)
        : storage(new storage_t(rows, columns, serialized)), view(){
    }

    /**
     * Copy constructor generating shallow copy.
     *
     * @param matrix original
     */
    Matrix(const self_t &matrix)
        : storage(matrix.storage
            ? matrix.array2d()->storage_t::copy(false)
            : NULL),
        view(matrix.view){}

    template <class T2, template <class> class Array2D_Type2>
    Matrix(const Matrix<T2, Array2D_Type2, ViewType> &matrix)
        : storage(matrix.storage
            ? new storage_t(matrix.storage)
            : NULL),
        view(matrix.view) {}
  protected:
    template <class ViewType2>
    Matrix(const Matrix<T, Array2D_Type, ViewType2> &matrix)
        : storage(matrix.storage
            ? matrix.array2d()->storage_t::copy(false)
            : NULL),
        view(matrix.view) {}

  public:
    /**
     * Destructor
     */
    virtual ~Matrix(){delete storage;}


    /**
     * Matrix generator with specified row and column numbers.
     * The storage will be assigned with the size,
     * however, initialization of elements will NOT be performed.
     * In addition, its view is none.
     *
     * @param new_rows Row number
     * @param new_columns Column number
     */
    static viewless_t blank(
        const unsigned int &new_rows,
        const unsigned int &new_columns){
      return viewless_t(new storage_t(new_rows, new_columns));
    }

  protected:
    viewless_t blank_copy() const {
      return blank(rows(), columns());
    }

  public:
    /**
     * Assign operator performing shallow copy.
     *
     * @return myself
     */
    self_t &operator=(const self_t &matrix){
      if(this != &matrix){
        delete storage;
        if(matrix.storage){
          storage = matrix.array2d()->storage_t::copy(false);
        }
        view = matrix.view;
      }
      return *this;
    }
    template <class T2, template <class> class Array2D_Type2>
    self_t &operator=(const Matrix<T2, Array2D_Type2, ViewType> &matrix){
      delete storage;
      storage = new storage_t(*matrix.storage);
      view = matrix.view;
      return *this;
    }

    /**
     * Perform (deep) copy
     *
     * @return (viewless_t)
     */
    viewless_t copy() const {
      if(view.is_viewless()){
        return viewless_t(array2d()->storage_t::copy(true));
      }else{
        viewless_t res(blank_copy());
        for(unsigned int i(0); i < rows(); ++i){
          for(unsigned int j(0); j < columns(); ++j){
            res(i, j) = (*this)(i, j);
          }
        }
        return res;
      }
    }
    
    /**
     * Test whether elements are identical
     * 
     * @param matrix Matrix to be compared
     * @return true when elements of two matrices are identical, otherwise false.
     */
    template <
        class T2, template <class> class Array2D_Type2,
        class ViewType2>
    bool operator==(const Matrix<T2, Array2D_Type2, ViewType2> &matrix) const {
      if(this == &matrix){return true;}
      if((rows() != matrix.rows())
          || columns() != matrix.columns()){
        return false;
      }
      for(unsigned int i(0); i < rows(); i++){
        for(unsigned int j(0); j < columns(); j++){
          if((*this)(i, j) != matrix(i, j)){
            return false;
          }
        }
      }
      return true;
    }
    
    template <
        class T2, template <class> class Array2D_Type2,
        class ViewType2>
    bool operator!=(const Matrix<T2, Array2D_Type2, ViewType2> &matrix) const {
      return !(operator==(matrix));
    }

    /**
     * Generate scalar matrix
     *
     * @param size Row and column number
     * @param scalar
     */
    static viewless_t getScalar(const unsigned int &size, const T &scalar){
      viewless_t result(size, size);
      for(unsigned int i(0); i < size; i++){result(i, i) = scalar;}
      return result;
    }

    /**
     * Generate unit matrix
     *
     * @param size Row and column number
     */
    static viewless_t getI(const unsigned int &size){
      return getScalar(size, T(1));
    }

    /**
     * Generate transpose matrix
     * Be careful, the return value is linked to the original matrix.
     * In order to unlink, do transpose().copy().
     *
     * @return Transpose matrix
     */
    transposed_t transpose() const{
      return transposed_t(*this);
    }

    /**
     * Generate partial matrix
     * Be careful, the return value is linked to the original matrix.
     * In order to unlink, do partial().copy().
     *
     * @param rowSize Row number
     * @param columnSize Column number
     * @param rowOffset Upper row index of original matrix for partial matrix
     * @param columnOffset Left column index of original matrix for partial matrix
     * @return partial matrix
     *
     */
    partial_t partial(
        const unsigned int &new_rows,
        const unsigned int &new_columns,
        const unsigned int &row_offset,
        const unsigned int &column_offset) const throw(MatrixException) {
      partial_t res(*this);
      if(!view.partialized){
        res.view.partial.row_offset = res.view.partial.column_offset = 0;
      }
      if(view.transposed){
        res.view.partial.rows = new_columns;
        res.view.partial.columns = new_rows;
        res.view.partial.row_offset += column_offset;
        res.view.partial.column_offset += row_offset;
      }else{
        res.view.partial.rows = new_rows;
        res.view.partial.columns = new_columns;
        res.view.partial.row_offset += row_offset;
        res.view.partial.column_offset += column_offset;
      }
      if(((res.view.partial.row_offset + res.view.partial.rows) > storage->rows())
          || ((res.view.partial.column_offset + res.view.partial.columns) > storage->columns())){
        throw MatrixException("size exceeding");
      }
      return res;
    }

    /**
     * Generate row vector by using partial()
     *
     * @param row Row index of original matrix for row vector
     * @return Row vector
     * @see partial()
     */
    partial_t rowVector(const unsigned int &row) const {
      return partial(1, columns(), row, 0);
    }
    /**
     * Generate column vector by using partial()
     *
     * @param column Column index of original matrix for column vector
     * @return Column vector
     * @see partial()
     */
    partial_t columnVector(const unsigned int &column) const {
      return partial(rows(), 1, 0, column);
    }

    /**
     * Exchange rows (bang method).
     *
     * @param row1 Target row (1)
     * @param row2 Target row (2)
     * @return myself
     * @throw MatrixException
     */
    self_t &exchangeRows(
        const unsigned int &row1, const unsigned int &row2) throw(MatrixException){
      if(row1 >= rows() || row2 >= rows()){throw MatrixException("Index incorrect");}
      T temp;
      for(unsigned int j(0); j < columns(); j++){
        temp = (*this)(row1, j);
        (*this)(row1, j) = (*this)(row2, j);
        (*this)(row2, j) = temp;
      }
      return *this;
    }

    /**
     * Exchange columns (bang method).
     *
     * @param column1 Target column (1)
     * @param column2 Target column (2)
     * @return myself
     * @throw MatrixException
     */
    self_t &exchangeColumns(
        const unsigned int &column1, const unsigned int &column2){
      if(column1 >= columns() || column2 >= columns()){throw MatrixException("Index incorrect");}
      T temp;
      for(unsigned int i(0); i < rows(); i++){
        temp = (*this)(i, column1);
        (*this)(i, column1) = (*this)(i, column2);
        (*this)(i, column2) = temp;
      }
      return *this;
    }

    /**
     * Test whether matrix is square
     *
     * @return true when square, otherwise false.
     */
    bool isSquare() const{return rows() == columns();}

    /**
     * Test whether matrix is diagonal
     *
     * @return true when diagonal, otherwise false.
     */
    bool isDiagonal() const{
      if(isSquare()){
        for(unsigned int i(0); i < rows(); i++){
          for(unsigned int j(i + 1); j < columns(); j++){
            if(((*this)(i, j) != T(0)) || ((*this)(j, i) != T(0))){
              return false;
            }
          }
        }
        return true;
      }else{return false;}
    }

    /**
     * Test whether matrix is symmetric
     *
     * @return true when symmetric, otherwise false.
     */
    bool isSymmetric() const{
      if(isSquare()){
        for(unsigned int i(0); i < rows(); i++){
          for(unsigned int j(i + 1); j < columns(); j++){
            if((*this)(i, j) != (*this)(j, i)){return false;}
          }
        }
        return true;
      }else{return false;}
    }

    /**
     * Test whether size of matrices is different
     *
     * @param matrix Matrix to be compared
     * @return true when size different, otherwise false.
     */
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    bool isDifferentSize(const Matrix<T2, Array2D_Type2, ViewType2> &matrix) const{
      return (rows() != matrix.rows()) || (columns() != matrix.columns());
    }

  protected:
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    self_t &replace_internal(const Matrix<T2, Array2D_Type2, ViewType2> &matrix){
      for(unsigned int i(0); i < rows(); ++i){
        for(unsigned int j(0); j < columns(); ++j){
          (*this)(i, j) = matrix(i, j);
        }
      }
      return *this;
    }

  public:
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    self_t &replace(
        const Matrix<T2, Array2D_Type2, ViewType2> &matrix,
        const bool &do_check = true)
        throw(MatrixException) {
      if(do_check && isDifferentSize(matrix)){
        throw MatrixException("Incorrect size");
      }
      return replace_internal(matrix);
    }

    /**
     * Return trace of matrix
     *
     * @param do_check Check matrix size property. The default is true
     * @return Trace
     */
    T trace(const bool &do_check = true) const throw(MatrixException) {
      if(do_check && !isSquare()){throw MatrixException("rows != columns");}
      T tr(0);
      for(unsigned i(0); i < rows(); i++){
        tr += (*this)(i, i);
      }
      return tr;
    }

    /**
     * Multiple by scalar (bang method)
     *
     * @param scalar
     * @return myself
     */
    self_t &operator*=(const T &scalar){
      for(unsigned int i(0); i < rows(); i++){
        for(unsigned int j(0); j < columns(); j++){
          (*this)(i, j) *= scalar;
        }
      }
      return *this;
    }
    /**
     * Multiple by scalar
     *
     * @param scalar
     * @return multiplied (deep) copy
     */
    viewless_t operator*(const T &scalar) const{return (copy() *= scalar);}
    /**
     * Multiple by scalar
     *
     * @param scalar
     * @param matrix
     * @return multiplied (deep) copy
     */
    friend viewless_t operator*(const T &scalar, const self_t &matrix){
      return matrix * scalar;
    }
    /**
     * Divide by scalar (bang method)
     *
     * @param scalar
     * @return myself
     */
    self_t &operator/=(const T &scalar){return (*this) *= (1 / scalar);}
    /**
     * Divide by scalar
     *
     * @param scalar
     * @return divided (deep) copy
     */
    viewless_t operator/(const T &scalar) const{return (copy() /= scalar);}
    /**
     * Divide by scalar
     *
     * @param scalar
     * @param matrix
     * @return divided (deep) copy
     */
    friend viewless_t operator/(const T &scalar, const self_t &matrix){
      return matrix / scalar;
    }
    
    /**
     * Add by matrix (bang method)
     *
     * @param matrix Matrix to add
     * @return myself
     */
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    self_t &operator+=(const Matrix<T2, Array2D_Type2, ViewType2> &matrix) throw(MatrixException) {
      if(isDifferentSize(matrix)){throw MatrixException("Incorrect size");}
      for(unsigned int i(0); i < rows(); i++){
        for(unsigned int j(0); j < columns(); j++){
          (*this)(i, j) += matrix(i, j);
        }
      }
      return *this;
    }

    /**
     * Add by matrix
     *
     * @param matrix Matrix to add
     * @return added (deep) copy
     */
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    viewless_t operator+(const Matrix<T2, Array2D_Type2, ViewType2> &matrix) const{
      return (copy() += matrix);
    }
    
    /**
     * Subtract by matrix (bang method)
     *
     * @param matrix Matrix to subtract
     * @return myself
     */
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    self_t &operator-=(const Matrix<T2, Array2D_Type2, ViewType2> &matrix) throw(MatrixException) {
      if(isDifferentSize(matrix)){throw MatrixException("Incorrect size");}
      for(unsigned int i(0); i < rows(); i++){
        for(unsigned int j(0); j < columns(); j++){
          (*this)(i, j) -= matrix(i, j);
        }
      }
      return *this;
    }

    /**
     * Subtract by matrix
     *
     * @param matrix Matrix to subtract
     * @return subtracted (deep) copy
     */
    template <class T2, template <class> class Array2D_Type2>
    viewless_t operator-(const Matrix<T2, Array2D_Type2> &matrix) const{
      return (copy() -= matrix);
    }

    /**
     * Multiply by matrix
     *
     * @param matrix matrix to multiply
     * @return multiplied (deep) copy
     * @throw MatrixException
     */
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    viewless_t operator*(const Matrix<T2, Array2D_Type2, ViewType2> &matrix) const throw(MatrixException){
      if(columns() != matrix.rows()){
        throw MatrixException("Incorrect size");
      }
      viewless_t result(blank(rows(), matrix.columns()));
      for(unsigned int i(0); i < result.rows(); i++){
        for(unsigned int j(0); j < result.columns(); j++){
          result(i, j) = (*this)(i, 0) * matrix(0, j);
          for(unsigned int k(1); k < columns(); k++){
            result(i, j) += ((*this)(i, k) * matrix(k, j));
          }
        }
      }
      return result;
    }
    
    /**
     * Multiply by matrix (bang method)
     *
     * @param matrix Matrix to multiply
     * @return myself
     * @throw MatrixException
     */
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    self_t &operator*=(const Matrix<T2, Array2D_Type2, ViewType2> &matrix){
      return replace_internal(*this * matrix);
    }

    /**
     * Unary minus operator, which is alias of matrix * -1.
     *
     * @return matrix * -1.
     */
    viewless_t operator-() const{return (copy() *= -1);}

    /**
     * TODO
     *
     * 補行列(余因子行列)を求めます。
     *
     * @param row 行インデックス
     * @param column 列インデックス
     * @return (CoMatrix) 補行列
     */
#if 0
    CoMatrix<T> coMatrix(
        const unsigned int &row,
        const unsigned int &column) const {
      return CoMatrix<T>(*this, row, column);
    }
#else
    viewless_t coMatrix(
        const unsigned int &row,
        const unsigned int &column) const {
      viewless_t res(blank(rows() - 1, columns() - 1));
      unsigned int i(0), i2(0);
      for( ; i < row; ++i, ++i2){
        unsigned int j(0), j2(0);
        for( ; j < column; ++j, ++j2){
          res(i, j) = operator()(i2, j2);
        }
        ++j2;
        for( ; j < res.columns(); ++j, ++j2){
          res(i, j) = operator()(i2, j2);
        }
      }
      ++i2;
      for( ; i < res.rows(); ++i, ++i2){
        unsigned int j(0), j2(0);
        for( ; j < column; ++j, ++j2){
          res(i, j) = operator()(i2, j2);
        }
        ++j2;
        for( ; j < res.columns(); ++j, ++j2){
          res(i, j) = operator()(i2, j2);
        }
      }
      return res;
    }
#endif

    /**
     * Calculate determinant
     *
     * @param do_check Whether check size property. The default is true.
     * @return Determinant
     * @throw MatrixException
     */
    T determinant(const bool &do_check = true) const throw(MatrixException){
      if(do_check && !isSquare()){throw MatrixException("rows() != columns()");}
      if(rows() == 1){
        return (*this)(0, 0);
      }else{
        T sum(0);
        T sign(1);
        for(unsigned int i(0); i < rows(); i++){
          if((*this)(i, 0) != T(0)){
            sum += (*this)(i, 0) * (coMatrix(i, 0).determinant(false)) * sign;
          }
          sign = -sign;
        }
        return sum;
      }
    }

    /**
     * Resolve x of (Ax = y), where this matrix is A and has already been decomposed as LU.
     *
     * @param y Right hand term
     * @param do_check Check whether already LU decomposed
     * @return Left hand second term
     * @see decomposeLU(const bool &)
     */
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    typename Matrix<T2, Array2D_Type2, ViewType2>::viewless_t
        solve_linear_eq_with_LU(
            const Matrix<T2, Array2D_Type2, ViewType2> &y, const bool &do_check = true)
            const throw(MatrixException) {
      bool not_LU(false);
      if(do_check){
        if(rows() * 2 != columns()){not_LU = true;}
      }
      partial_t
          L(partial(rows(), rows(), 0, 0)),
          U(partial(rows(), rows(), 0, rows()));
      if(do_check){
        for(unsigned i(1); i < rows(); i++){
          for(unsigned j(0); j < i; j++){
            if(U(i, j) != T(0)){not_LU = true;}
            if(L(j, i) != T(0)){not_LU = true;}
          }
        }
      }
      if(not_LU){
        throw MatrixException("Not LU decomposed matrix!!");
      }
      if((y.columns() != 1)
          || (y.rows() != rows())){
        throw MatrixException("Incorrect y size");
      }


      typedef typename Matrix<T2, Array2D_Type2>::viewless_t y_t;
      // L(Ux) = y で y' = (Ux)をまず解く
      y_t y_copy(y.copy());
      y_t y_prime(y_t::blank(y.rows(), 1));
      for(unsigned i(0); i < rows(); i++){
        y_prime(i, 0) = y_copy(i, 0) / L(i, i);
        for(unsigned j(i + 1); j < rows(); j++){
          y_copy(j, 0) -= L(j, i) * y_prime(i, 0);
        }
      }

      // 続いてUx = y'で xを解く
      y_t x(y_t::blank(y.rows(), 1));
      for(unsigned i(rows()); i > 0;){
        i--;
        x(i, 0) = y_prime(i, 0) / U(i, i);
        for(unsigned j(i); j > 0;){
          j--;
          y_prime(j, 0) -= U(j, i) * x(i, 0);
        }
      }

      return x;
    }

    /**
     * Perform decomposition of LU
     * Return matrix is
     * (0, 0)-(n-1, n-1):  L matrix
     * (0, n)-(n-1, 2n-1): U matrix
     *
     * @param do_check Check size, the default is true.
     * @return LU decomposed matrix
     * @throw MatrixException
     */
    viewless_t decomposeLU(const bool &do_check = true) const throw(MatrixException){
      if(do_check && !isSquare()){throw MatrixException("rows() != columns()");}
      viewless_t LU(blank(rows(), columns() * 2));
#define L(i, j) LU(i, j)
#define U(i, j) LU(i, j + columns())
      for(unsigned int i(0); i < rows(); i++){
        for(unsigned int j(0); j < columns(); j++){
          if(i >= j){
           U(i, j) = T(i == j ? 1 : 0);
            L(i, j) = (*this)(i, j);
            for(unsigned int k(0); k < j; k++){
              L(i, j) -= (L(i, k) * U(k, j));
            }
          }else{
           L(i, j) = T(0);
            U(i, j) = (*this)(i, j);
            for(unsigned int k(0); k < i; k++){
              U(i, j) -= (L(i, k) * U(k, j));
            }
            U(i, j) /= L(i, i);
          }
        }
      }
#undef L
#undef U
      return LU;
    }

    /**
     * Perform UD decomposition
     * Return matrix is
     * (0, 0)-(n-1,n-1):  U matrix
     * (0, n)-(n-1,2n-1): D matrix
     *
     * @param do_check Check size, the default is true.
     * @return UD decomposed matrix
     * @throw MatrixException
     */
    viewless_t decomposeUD(const bool &do_check = true) const throw(MatrixException){
      if(do_check && !isSymmetric()){throw MatrixException("not symmetric");}
      viewless_t P(copy());
      viewless_t UD(rows(), columns() * 2);
#define U(i, j) UD(i, j)
#define D(i, j) UD(i, j + columns())
      for(int i(rows() - 1); i >= 0; i--){
        D(i, i) = P(i, i);
        U(i, i) = T(1);
        for(unsigned int j(0); j < i; j++){
          U(j, i) = P(j, i) / D(i, i);
          for(unsigned int k(0); k <= j; k++){
            P(k, j) -= U(k, i) * D(i, i) * U(j, i);
          }
        }
      }
#undef U
#undef D
      return UD;
    }

    /**
     * Calculate inverse matrix
     *
     * @return Inverse matrix
     * @throw MatrixException
     */
    viewless_t inverse() const throw(MatrixException){

      if(!isSquare()){throw MatrixException("rows() != columns()");}

      //クラメール(遅い)
      /*
      viewless_t result(rows(), columns());
      T det;
      if((det = determinant()) == 0){throw MatrixException("Operation void!!");}
      for(unsigned int i(0); i < rows(); i++){
        for(unsigned int j(0); j < columns(); j++){
          result(i, j) = coMatrix(i, j).determinant() * ((i + j) % 2 == 0 ? 1 : -1);
        }
      }
      return result.transpose() / det;
      */

      //ガウス消去法

      viewless_t left(copy());
      viewless_t right(viewless_t::getI(rows()));
      for(unsigned int i(0); i < rows(); i++){
        if(left(i, i) == T(0)){ //(i, i)が存在するように並べ替え
          for(unsigned int j(i + 1); j <= rows(); j++){
            if(j == rows()){throw MatrixException("invert matrix not exist");}
            if(left(j, i) != T(0)){
              left.exchangeRows(j, i);
              right.exchangeRows(j, i);
              break;
            }
          }
        }
        if(left(i, i) != T(1)){
          for(unsigned int j(0); j < columns(); j++){right(i, j) /= left(i, i);}
          for(unsigned int j(i+1); j < columns(); j++){left(i, j) /= left(i, i);}
          left(i, i) = T(1);
        }
        for(unsigned int k(0); k < rows(); k++){
          if(k == i){continue;}
          if(left(k, i) != T(0)){
            for(unsigned int j(0); j < columns(); j++){right(k, j) -= right(i, j) * left(k, i);}
            for(unsigned int j(i+1); j < columns(); j++){left(k, j) -= left(i, j) * left(k, i);}
            left(k, i) = T(0);
          }
        }
      }
      //std::cout << "L:" << left << std::endl;
      //std::cout << "R:" << right << std::endl;

      return right;

      //LU分解
      /*
      */
    }
    /**
     * Divide by matrix, in other words, multiply by inverse matrix. (bang method)
     *
     * @param matrix Matrix to divide
     * @return myself
     */
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    self_t &operator/=(const Matrix<T2, Array2D_Type2, ViewType2> &matrix) {
        return (*this) *= matrix.inverse();
    }
    /**
     * Divide by matrix, in other words, multiply by inverse matrix
     *
     * @param matrix Matrix to divide
     * @return divided (deep) copy
     */
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    viewless_t operator/(const Matrix<T2, Array2D_Type2, ViewType2> &matrix) const {
      return (copy() /= matrix);
    }

    /**
     * Add by matrix with specified pivot (bang method)
     *
     * @param row Upper row index (pivot) of matrix to be added
     * @param column Left column index (pivot) of matrix to be added
     * @param matrix Matrix to add
     * @return myself
     */
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    self_t &pivotMerge(
        const unsigned int &row, const unsigned int &column,
        const Matrix<T2, Array2D_Type2, ViewType2> &matrix){
      for(int i(0); i < matrix.rows(); i++){
        if(row + i < 0){continue;}
        else if(row + i >= rows()){break;}
        for(int j(0); j < matrix.columns(); j++){
          if(column + j < 0){continue;}
          else if(column + j >= columns()){break;}
          (*this)(row + i, column + j) += matrix(i, j);
        }
      }
      return *this;
    }

    /**
     * Add by matrix with specified pivot
     *
     * @param row Upper row index (pivot) of matrix to be added
     * @param column Left column index (pivot) of matrix to be added
     * @param matrix Matrix to add
     * @return added (deep) copy
     */
    template <class T2, template <class> class Array2D_Type2, class ViewType2>
    viewless_t pivotAdd(
        const unsigned int &row, const unsigned int &column,
        const Matrix<T2, Array2D_Type2, ViewType2> &matrix) const{
      return copy().pivotMerge(row, column, matrix);
    }

    /**
     * Calculate Hessenberg matrix by performing householder conversion
     *
     * @param transform Pointer to store multiplication of matrices used for the conversion.
     * If NULL is specified, the store will not be performed, The default is NULL.
     * @return Hessenberg matrix
     * @throw MatrixException
     */
    viewless_t hessenberg(viewless_t *transform = NULL) const throw(MatrixException){
      if(!isSquare()){throw MatrixException("rows() != columns()");}

      viewless_t result(copy());
      for(unsigned int j(0); j < columns() - 2; j++){
        T t(0);
        for(unsigned int i(j + 1); i < rows(); i++){
          t += pow(result(i, j), 2);
        }
        T s = ::sqrt(t);
        if(result(j + 1, j) < 0){s *= -1;}

        viewless_t omega(blank(rows() - (j+1), 1));
        {
          for(unsigned int i(0); i < omega.rows(); i++){
            omega(i, 0) = result(j+i+1, j);
          }
          omega(0, 0) += s;
        }

        viewless_t P(viewless_t::getI(rows()));
        T denom(t + result(j + 1, j) * s);
        if(denom){
          P.pivotMerge(j+1, j+1, -(omega * omega.transpose() / denom));
        }

        result = P * result * P;
        if(transform){(*transform) *= P;}
      }

      //ゼロ処理
      bool sym = isSymmetric();
      for(unsigned int j(0); j < columns() - 2; j++){
        for(unsigned int i(j + 2); i < rows(); i++){
          result(i, j) = T(0);
          if(sym){result(j, i) = T(0);}
        }
      }

      return result;
    }

    template <class T2>
    struct complex_t {
      static const bool is_complex = false;
      typedef Complex<T2> v_t;
      typedef typename Matrix<Complex<T2>, Array2D_Type, ViewType>::viewless_t m_t;
    };
    template <class T2>
    struct complex_t<Complex<T2> > {
        static const bool is_complex = true;
      typedef Complex<T2> v_t;
      typedef typename Matrix<Complex<T2>, Array2D_Type, ViewType>::viewless_t m_t;
    };

    /**
     * Calculate eigenvalues of 2 by 2 partial matrix.
     *
     * @param row Upper row index of the partial matrix
     * @param column Left column index of the partial matrix
     * @param upper Eigenvalue (1)
     * @param lower Eigenvalue (2)
     */
    void eigen22(
        const unsigned int &row, const unsigned int &column,
        typename complex_t<T>::v_t &upper, typename complex_t<T>::v_t &lower) const {
      T a((*this)(row, column)),
        b((*this)(row, column + 1)),
        c((*this)(row + 1, column)),
        d((*this)(row + 1, column + 1));
      T root2(pow((a - d), 2) + b * c * 4);
      if(complex_t<T>::is_complex || (root2 > 0)){
        T root(::sqrt(root2));
        upper = ((a + d + root) / 2);
        lower = ((a + d - root) / 2);
      }else{
        T root(::sqrt(root2 * -1));
        upper = typename complex_t<T>::v_t((a + d) / 2, root / 2);
        lower = typename complex_t<T>::v_t((a + d) / 2, root / 2 * -1);
      }
    }

    /**
     * Calculate eigenvalues and eigenvectors.
     * The return matrix consists of
     * (0,j)-(n-1,j): Eigenvector (j) (0 <= j <= n-1)
     * (j,n)-(j,n): Eigenvalue (j)
     *
     * @param threshold_abs Absolute error to be used for convergence determination
     * @param threshold_rel Relative error to be used for convergence determination
     * @return Eigenvalues and eigenvectors
     */
    typename complex_t<T>::m_t eigen(
        const T &threshold_abs = 1E-10,
        const T &threshold_rel = 1E-7) const throw(MatrixException){

      typedef typename complex_t<T>::m_t res_t;

      if(!isSquare()){throw MatrixException("rows() != columns()");}

      //パワー法(べき乗法)
      /*viewless_t result(rows(), rows() + 1);
      viewless_t source = copy();
      for(unsigned int i(0); i < columns(); i++){result(0, i) = T(1);}
      for(unsigned int i(0); i < columns(); i++){
        while(true){
          viewless_t approxVec = source * result.columnVector(i);
          T approxVal(0);
          for(unsigned int j(0); j < approxVec.rows(); j++){approxVal += pow(approxVec(j, 0), 2);}
          approxVal = sqrt(approxVal);
          for(unsigned int j(0); j < approxVec.rows(); j++){result(j, i) = approxVec(j, 0) / approxVal;}
          T before = result(i, rows());
          if(abs(before - (result(i, rows()) = approxVal)) < threshold){break;}
        }
        for(unsigned int j(0); (i < rows() - 1) && (j < rows()); j++){
          for(unsigned int k(0); k < rows(); k++){
            source(j, k) -= result(i, rows()) * result(j, i) * result(k, i);
          }
        }
      }
      return result;*/

      //ダブルQR法
      /* <手順>
       * ハウスホルダー法を適用して、上ヘッセンベルク行列に置換後、
       * ダブルQR法を適用。
       * 結果、固有値が得られるので、固有ベクトルを計算。
       */

      const unsigned int &_rows(rows());

      //結果の格納用の行列
      res_t result(_rows, _rows + 1);

      //固有値の計算
#define lambda(i) result(i, _rows)

      T mu_sum(0), mu_multi(0);
      typename complex_t<T>::v_t p1, p2;
      int m = _rows;
      bool first = true;

      viewless_t transform(getI(_rows));
      viewless_t A(hessenberg(&transform));
      viewless_t A_(A);

      while(true){

        //m = 1 or m = 2
        if(m == 1){
          lambda(0) = A(0, 0);
          break;
        }else if(m == 2){
          A.eigen22(0, 0, lambda(0), lambda(1));
          break;
        }

        //μ、μ*の更新(4.143)
        {
          typename complex_t<T>::v_t p1_new, p2_new;
          A.eigen22(m-2, m-2, p1_new, p2_new);
          if(first ? (first = false) : true){
            if((p1_new - p1).abs() > p1_new.abs() / 2){
              if((p2_new - p2).abs() > p2_new.abs() / 2){
                mu_sum = (p1 + p2).real();
                mu_multi = (p1 * p2).real();
              }else{
                mu_sum = p2_new.real() * 2;
                mu_multi = pow(p2_new.real(), 2);
              }
            }else{
              if((p2_new - p2).abs() > p2_new.abs() / 2){
                mu_sum = p1_new.real() * 2;
                mu_multi = p1_new.real() * p1_new.real();
              }else{
                mu_sum = (p1_new + p2_new).real();
                mu_multi = (p1_new * p2_new).real();
              }
            }
          }
          p1 = p1_new, p2 = p2_new;
        }

        //ハウスホルダー変換を繰り返す
        T b1, b2, b3, r;
        for(int i(0); i < m - 1; i++){
          if(i == 0){
            b1 = A(0, 0) * A(0, 0) - mu_sum * A(0, 0) + mu_multi + A(0, 1) * A(1, 0);
            b2 = A(1, 0) * (A(0, 0) + A(1, 1) - mu_sum);
            b3 = A(2, 1) * A(1, 0);
          }else{
            b1 = A(i, i - 1);
            b2 = A(i + 1, i - 1);
            b3 = (i == m - 2 ? T(0) : A(i + 2, i - 1));
          }

          r = ::sqrt((b1 * b1) + (b2 * b2) + (b3 * b3));

          viewless_t omega(3, 1);
          {
            omega(0, 0) = b1 + r * (b1 >= T(0) ? 1 : -1);
            omega(1, 0) = b2;
            if(b3 != T(0)){omega(2, 0) = b3;}
          }
          viewless_t P(viewless_t::getI(_rows));
          T denom((omega.transpose() * omega)(0, 0));
          if(denom){
            P.pivotMerge(i, i, omega * omega.transpose() * -2 / denom);
          }
          //std::cout << "denom(" << m << ") " << denom << std::endl;

          A = P * A * P;
        }
        //std::cout << "A_scl(" << m << ") " << A(m-1,m-2) << std::endl;

        if(std::isnan(A(m-1,m-2)) || !std::isfinite(A(m-1,m-2))){
          throw MatrixException("eigen values calculation failed");
        }

        //収束判定
#define _abs(x) ((x) >= 0 ? (x) : -(x))
        T A_m2_abs(_abs(A(m-2, m-2))), A_m1_abs(_abs(A(m-1, m-1)));
        T epsilon(threshold_abs
          + threshold_rel * ((A_m2_abs < A_m1_abs) ? A_m2_abs : A_m1_abs));

        //std::cout << "epsil(" << m << ") " << epsilon << std::endl;

        if(_abs(A(m-1, m-2)) < epsilon){
          --m;
          lambda(m) = A(m, m);
        }else if(_abs(A(m-2, m-3)) < epsilon){
          A.eigen22(m-2, m-2, lambda(m-1), lambda(m-2));
          m -= 2;
        }
      }
#undef _abs

#if defined(MATRIX_EIGENVEC_SIMPLE)
      //固有ベクトルの計算
      res_t x(_rows, _rows);  //固有ベクトル
      A = A_;

      for(unsigned int j(0); j < _rows; j++){
        unsigned int n = _rows;
        for(unsigned int i(0); i < j; i++){
          if((lambda(j) - lambda(i)).abs() <= threshold_abs){--n;}
        }
        //std::cout << n << ", " << lambda(j) << std::endl;
        x(--n, j) = 1;
        while(n-- > 0){
          x(n, j) = x(n+1, j) * (lambda(j) - A(n+1, n+1));
          for(unsigned int i(n+2); i < _rows; i++){
            x(n, j) -= x(i, j) * A(n+1, i);
          }
          if(A(n+1, n)){x(n, j) /= A(n+1, n);}
        }
        //std::cout << x.partial(_rows, 1, 0, j).transpose() << std::endl;
      }
#else
      //固有ベクトルの計算(逆反復法)
      res_t x(res_t::getI(_rows));  //固有ベクトル
      A = A_;
      res_t A_C(_rows, _rows);
      for(unsigned int i(0); i < _rows; i++){
        for(unsigned int j(0); j < columns(); j++){
          A_C(i, j) = A(i, j);
        }
      }

      for(unsigned int j(0); j < _rows; j++){
        // http://www.prefield.com/algorithm/math/eigensystem.html を参考に
        // かつ、固有値が等しい場合の対処方法として、
        // http://www.nrbook.com/a/bookcpdf/c11-7.pdf
        // を参考に、値を振ってみることにした
        res_t A_C_lambda(A_C.copy());
        typename complex_t<T>::v_t approx_lambda(lambda(j));
        if((A_C_lambda(j, j) - approx_lambda).abs() <= 1E-3){
          approx_lambda += 2E-3;
        }
        for(unsigned int i(0); i < _rows; i++){
          A_C_lambda(i, i) -= approx_lambda;
        }
        res_t A_C_lambda_LU(A_C_lambda.decomposeLU());

        res_t target_x(res_t::blank(_rows, 1));
        for(unsigned i(0); i < _rows; ++i){
          target_x(i, 0) = x(i, j);
        }
        for(unsigned loop(0); true; loop++){
          res_t target_x_new(
              A_C_lambda_LU.solve_linear_eq_with_LU(target_x, false));
          T mu((target_x_new.transpose() * target_x)(0, 0).abs2()),
            v2((target_x_new.transpose() * target_x_new)(0, 0).abs2()),
            v2s(::sqrt(v2));
          for(unsigned i(0); i < _rows; ++i){
            target_x(i, 0) = target_x_new(i, 0) / v2s;
          }
          //std::cout << mu << ", " << v2 << std::endl;
          //std::cout << target_x.transpose() << std::endl;
          if((T(1) - (mu * mu / v2)) < T(1.1)){
            for(unsigned i(0); i < _rows; ++i){
              x(i, j) = target_x(i, 0);
            }
            break;
          }
          if(loop > 100){
            throw MatrixException("eigen vectors calculation failed");
          }
        }
      }
#endif

      /*res_t lambda2(_rows, _rows);
      for(unsigned int i(0); i < _rows; i++){
        lambda2(i, i) = lambda(i);
      }

      std::cout << "A:" << A << std::endl;
      //std::cout << "x * x^-1" << x * x.inverse() << std::endl;
      std::cout << "x * lambda * x^-1:" << x * lambda2 * x.inverse() << std::endl;*/

      //結果の格納
      for(unsigned int j(0); j < x.columns(); j++){
        for(unsigned int i(0); i < x.rows(); i++){
          for(unsigned int k(0); k < transform.columns(); k++){
            result(i, j) += transform(i, k) * x(k, j);
          }
        }

        //正規化
        typename complex_t<T>::v_t _norm;
        for(unsigned int i(0); i < _rows; i++){
          _norm += result(i, j).abs2();
        }
        T norm = ::sqrt(_norm.real());
        for(unsigned int i(0); i < _rows; i++){
          result(i, j) /= norm;
        }
        //std::cout << result.partial(_rows, 1, 0, j).transpose() << std::endl;
      }
#undef lambda

      return result;
    }

  protected:
    /**
     * Calculate square root of a matrix
     *
     * If matrix (A) can be decomposed as
     * @f[
     *    A = V D V^{-1},
     * @f]
     * where D and V are diagonal matrix consisting of eigenvalues and eigenvectors, respectively,
     * the square root A^{1/2} is
     * @f[
     *    A^{1/2} = V D^{1/2} V^{-1}.
     * @f]
     *
     * @param eigen_mat result of eigen()
     * @return square root
     * @see eiegn(const T &, const T &)
     */
    static typename complex_t<T>::m_t sqrt(
        const typename complex_t<T>::m_t &eigen_mat){
      unsigned int n(eigen_mat.rows());
      typename complex_t<T>::m_t::partial_t VsD(eigen_mat.partial(n, n, 0, 0));
      typename complex_t<T>::m_t nV(VsD.inverse());
      for(unsigned int i(0); i < n; i++){
        VsD.partial(n, 1, 0, i) *= (eigen_mat(i, n).sqrt());
      }

      return VsD * nV;
    }

  public:
    /**
     * Calculate square root of a matrix
     *
     * @param threshold_abs Absolute error to be used for convergence determination of eigenvalue calculation
     * @param threshold_abs Relative error to be used for convergence determination of eigenvalue calculation
     * @return square root
     * @see eigen(const T &, const T &)
     */
    typename complex_t<T>::m_t sqrt(
        const T &threshold_abs,
        const T &threshold_rel) const {
      return sqrt(eigen(threshold_abs, threshold_rel));
    }

    /**
     * Calculate square root
     *
     * @return square root
     */
    typename complex_t<T>::m_t sqrt() const {
      return sqrt(eigen());
    }

    /**
     * Print matrix
     *
     */
    friend std::ostream &operator<<(std::ostream &out, const self_t &matrix){
      if(matrix.storage){
        out << "{";
        for(unsigned int i(0); i < matrix.rows(); i++){
          out << (i == 0 ? "" : ",") << std::endl << "{";
          for(unsigned int j(0); j < matrix.columns(); j++){
            out << (j == 0 ? "" : ",") << matrix(i, j);
          }
          out << "}";
        }
        out << std::endl << "}";
      }
      return out;
    }
};

#undef throw_when_debug

#endif /* __MATRIX_H */
