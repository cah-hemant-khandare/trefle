#include "fiseval_bindings.h"

py::array_t<double> FISCocoEvalWrapper::predict_c(const string &ind_sp1,
                                                  const string &ind_sp2) {
  // double predict_c(const string &ind_sp1, const string &ind_sp2) {
  cout << "ind_sp1 " << ind_sp1 << " (" << ind_sp1.length() << ")" << endl;
  cout << "ind_sp2 " << ind_sp2 << " (" << ind_sp2.length() << ")" << endl;

  /// Parse ind_sp1
  // const vector<LinguisticVariable> vec_lv = parse_ind_sp1(ind_sp1);
  auto vec_lv = parse_ind_sp1(ind_sp1);

  /// Parse ind_sp2
  // TODO: parse ind_sp2

  /// Extract selected variables
  size_t offset = 0;
  auto sel_vars = extract_sel_vars(ind_sp2, offset);

  /// Extract linguistic variables
  auto r_lv = extract_r_lv(ind_sp2, offset);

  /// Extract rules/antecedents labels
  auto r_labels = extract_r_labels(ind_sp2, offset);

  /// Extract consequents
  auto r_cons = extract_r_cons(ind_sp2, offset);

  /// Combine ind_sp1 and ind_sp2 to create a FIS
  unordered_map<size_t, size_t> vars_lv_lookup;
  const size_t rows = sel_vars.size();
  const size_t cols = sel_vars[0].size();
  for (size_t i = 0; i < rows; i++) {
    for (size_t j = 0; j < cols; j++) {
      // key: a selected var, value: the matching lv
      // To respect the interpretability criteria i.e. a variable must have the
      // same definition (i.e. use the same lv) across all rules. This is done
      // by only keeping the last definition for a given variable.
      // We use the uniqueness feature of the map class to do that.
      vars_lv_lookup[sel_vars[i][j]] = r_lv[i][j];
    }
  }

  for (auto &t : vars_lv_lookup) {
    cout << t.first << ": " << t.second << endl;
  }

  /*
  auto rules = ...
  SingletonFIS fis(rules, default_rule)
  */

  /// Use this FIS and predict the output given X_train/new_X
  // TODO: create a predict_c(ind_sp1, ind_sp2, new_X) overriding method

  /// Return the y_pred to the caller
  // return y_pred

  cout << "predict done" << endl;

  return vec_lv;
}

template <typename T>
vector<vector<T>> FISCocoEvalWrapper::parse_bit_array(
    const string &bitarray, const size_t rows, const size_t cols,
    const size_t n_bits_per_elm,
    const std::function<T(const size_t, const size_t, const size_t)>
        &post_func) {

  vector<vector<T>> matrix(rows, vector<T>(cols, 0));

  const size_t n_bits_per_line = cols * n_bits_per_elm;
  for (size_t i = 0; i < rows; i++) {
    for (size_t j = 0; j < cols; j++) {
      const size_t offset = i * n_bits_per_line + (j * n_bits_per_elm);
      size_t value = stoi(bitarray.substr(offset, n_bits_per_elm), nullptr, 2);
      matrix[i][j] = post_func(value, i, j);
      // matrix[i][j] = value;
      cout << matrix[i][j] << ",";
    }
    cout << endl;
  }

  return matrix;
}

py::array_t<double> FISCocoEvalWrapper::parse_ind_sp1(const string &ind_sp1) {

  // // TODO: substr() is creating a new string each time, use c++17's
  // // stringview?

  const auto mf_val_to_01 = [&](const double v, const size_t i,
                                const size_t j) {
    // scale the mf p points to [0, 1] because it has been decided that
    // the observations (i.e. X_train) are MinMax normed.
    return v / double((1 << n_bits_per_mf) - 1);
  };

  vector<vector<double>> vec_lv = parse_bit_array<double>(
      ind_sp1, n_lv_per_ind, n_true_labels, n_bits_per_mf, mf_val_to_01);

  cout << "a" << endl;
  size_t mf_index = 1; // TODO remove me
  for (auto &row : vec_lv) {
    // create each LV
    // MF's points must be increasing
    sort(row.begin(), row.end());
    TriLV lv(row);
    cout << "lv : " << lv << endl;
    cout << "fuzzified to : " << lv.fuzzify(mf_index, 0.967742) << endl;
    for (const auto &col : row) {
      cout << col << ", ";
    }
    cout << endl;
  }

  // TODO: remove me, return instead y_pred
  auto arr = py::array_t<double>({n_lv_per_ind, n_true_labels});
  auto arr_raw = arr.mutable_unchecked<2>();

  for (size_t i = 0; i < n_lv_per_ind; i++) {
    for (size_t j = 0; j < n_true_labels; j++) {
      //// normalize in [0,1]
      // arr_raw(i, j) = vec_lv[i][j] / double((1 << n_bits_per_mf)-1);
      arr_raw(i, j) = vec_lv[i][j];
    }
  }
  return arr;
}

vector<vector<size_t>>
FISCocoEvalWrapper::extract_sel_vars(const string &ind_sp2, size_t &offset) {
  cout << "sel vars" << endl;
  const size_t n_bits_sel_vars = n_rules * n_max_vars_per_rule * n_bits_per_ant;
  cout << "c++ v: " << n_bits_sel_vars << endl;
  const auto val_to_var_idx = [&](const size_t v, const size_t i,
                                  const size_t j) {
    return modulo_trick(v, n_vars);
  };
  string sel_vars_bits = ind_sp2.substr(offset, n_bits_sel_vars);

  // move forward offset
  offset += n_bits_sel_vars;
  return parse_bit_array<size_t>(sel_vars_bits, n_rules, n_max_vars_per_rule,
                                 n_bits_per_ant, val_to_var_idx);
}

vector<vector<size_t>> FISCocoEvalWrapper::extract_r_lv(const string &ind_sp2,
                                                        size_t &offset) {
  cout << "r lv" << endl;
  const size_t n_bits_r_lv = n_rules * n_max_vars_per_rule * n_lv_per_ind;

  cout << "c++ v: " << n_bits_r_lv << endl;
  string r_lv_bits = ind_sp2.substr(offset, n_bits_r_lv);

  // move forward offset
  offset += n_bits_r_lv;

  // we use the dummy_post_func because we can use the parsed values as is
  // because n_lv_per_ind is a multiple of 2 so there is no need to do anything
  // in post processing.
  return parse_bit_array<size_t>(r_lv_bits, n_rules, n_max_vars_per_rule,
                                 n_lv_per_ind, dummy_post_func<size_t>);
}

vector<vector<size_t>>
FISCocoEvalWrapper::extract_r_labels(const string &ind_sp2, size_t &offset) {

  const auto val_to_label = [&](const size_t v, const size_t row,
                                const size_t col) {
    // This function scales the value v (which is in [0, n_bits_per_label-1])
    // to a label index (which is in [0,n_true_label] and where the last value
    // i.e. n_true_label represents a don't care label).
    //
    // If dc_weight = 0, v has the same probability to be either low, medium,
    // high,... but not don't care.
    //
    // If dc_weight = 1, v has the same probability to be either low, medium,
    // high,..., or don't care.
    //
    // If dc_weight = k, v has k times more chance to be a don't care than
    // the remaining labels (i.e. low, medium, high,...)
    //
    // Visually it is something like this.
    //
    //              <-------> <-------> <-------> <------->
    //                  j         j         j     dc_weight
    //             +                                       +
    //             |                                       |
    //             |         +         +         +         |
    //             |         |         |         |         |
    //             +---------+---------+---------+---------+
    //             0                                       1
    //                LOW      MEDIUM     HIGH       DC
    //
    // Here j is 1 / (n_true_labels+dc_weight). So if dc_weight > 1, the
    // probability to have a don't care increases.

    // v is in [0, (2^n_bits_per_label)-1]
    float v_normed = v / float((1 << n_bits_per_label) - 1);
    cout << "(" << v_normed << ", " << v << ")\t";

    // weights_normed is sth like [1,1,1,3] ≃ low, medium and high have a
    // weight of 1 and the last (i.e. don't care) have a weight of 3
    vector<float> weights_normed(n_true_labels + 1, 1);
    weights_normed[n_true_labels] = dc_weight;

    vector<int> indices;
    for (int i = 0; i < weights_normed.size(); i++) {
      for (int j = 0; j < weights_normed[i]; j++) {
        indices.push_back(i);
      }
    }

    int vec_n = indices.size();
    int idx = v_normed * vec_n;
    int safe_idx = max(0, min(idx, vec_n - 1));
    int to_ret = indices[int(safe_idx)];
    return to_ret;
  };

  cout << "r labels" << endl;
  const size_t n_bits_r_labels =
      n_rules * n_max_vars_per_rule * n_bits_per_label;
  cout << "c++ v: " << n_bits_r_labels << endl;

  string r_labels_bits = ind_sp2.substr(offset, n_bits_r_labels);

  // move forward offset
  offset += n_bits_r_labels;

  return parse_bit_array<size_t>(r_labels_bits, n_rules, n_max_vars_per_rule,
                                 n_bits_per_label, val_to_label);
}

vector<vector<double>> FISCocoEvalWrapper::extract_r_cons(const string &ind_sp2,
                                                          size_t &offset) {
  const auto val_to_cons = [&](const size_t v, const size_t rule_i,
                               const size_t cons_j) {
    //  cons_range is an array containing that number of classes per consequents
    // In the case of a consequent that use a continuous variable
    // (i.e. regression) it represents the number of labels used to cluster this
    // variables (i.e. like classes for a discrete variable)

    const size_t cons_max = cons_n_labels[cons_j];
    cout << "(" << cons_max << ")";
    return modulo_trick(v, cons_max);
  };

  cout << "r cons" << endl;
  const size_t n_bits_r_cons = n_rules * n_cons * n_bits_per_cons;
  string r_cons_bits = ind_sp2.substr(offset, n_bits_r_cons);

  // move forward offset
  offset += n_bits_r_cons;

  return parse_bit_array<double>(r_cons_bits, n_rules, n_cons, n_bits_per_cons,
                                 val_to_cons);
}
