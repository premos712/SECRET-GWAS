#include <math.h>

#include <limits>

#include "oblivious_logistic_regression.h"
#include "gwas.h"

/////////////////////////////////////////////////////////////
//////////              Oblivious_log_row             /////////////////
/////////////////////////////////////////////////////////////

// Approximates e^x from (-3, 3), and uses a step function after that. Good balance of accuracy and speed for our sigmoid function!
// https://math.stackexchange.com/questions/71357/approximation-of-e-x
double Oblivious_log_row::modified_pade_approx_oblivious(double x) {
    double approx = ((x + 3) * (x + 3) + 3) / ((x - 3) * (x - 3) + 3);
    int within_bounds = (x > -3) & (x < 3);
    return predicated_assignment(within_bounds, ((x > 0) << 7) * x, approx);
}

// 2 is a magic number that helps with SqrMatrix construction, "highest level matrix"
Oblivious_log_row::Oblivious_log_row(int _size, const std::vector<int>& sizes, GWAS* _gwas, ImputePolicy _impute_policy, int thread_id) : 
    Row(_size, sizes, _gwas->dim(), _impute_policy), H(num_dimensions, 2) {
    fitted = true;
    offset = thread_id * get_padded_buffer_len(num_dimensions);
    if (gwas->size() != n) throw CombineERROR("row length mismatch");
}

/* fitting */
bool Oblivious_log_row::fit(int thread_id, int max_it, double sig) {
    /* intialize beta to 0*/
    init();
    it_count = 1;

    while (it_count < max_it && bd_max(beta_delta_g + offset, num_dimensions) >= sig) {
        update_beta();
        it_count++;
    }

    if (it_count == max_it) {
        fitted = false;
        return false;
    }
    else {
        H.oblivious_INV();
        standard_error = std::sqrt(H.t[0][0]);
        return true;
    }
}

/* fitting helper functions */

void Oblivious_log_row::update_beta() {
    // calculate_beta
    H.oblivious_INV();
    H.calculate_t_matrix_times_vec(Grad_g + offset, beta_delta_g + offset);
    for (int i = 0; i < num_dimensions; i++) {
        double bd_i = (beta_delta_g + offset)[i];
        (beta_g + offset)[i] += bd_i;
        // take abs after adding beta delta so that we can determine if we have passed tolerance
        (beta_delta_g + offset)[i] = std::abs(bd_i);
    }

    update_estimate();
}

void Oblivious_log_row::init() {
    for (int i = 0; i < num_dimensions; ++i) {
        (beta_delta_g + offset)[i] = 1;
        (beta_g + offset)[i] = 0;
    }

    double sum = 0;
    double count = 0;
    uint8_t val;
    for (int i = 0 ; i < n; ++i) {
        val = (data[i / 4] >> ((i % 4) * 2)) & 0b11;
        int is_NA = is_NA_uint8(val);
        sum = predicated_assignment(is_NA, sum + val, sum);
        count = predicated_assignment(is_NA, count + 1, count);
    }

    genotype_average = sum / (count + !count);

    update_estimate();
}

void Oblivious_log_row::update_estimate() {
    for (int i = 0; i < num_dimensions; ++i) {
        (Grad_g + offset)[i] = 0;
        for (int j = 0; j < num_dimensions; ++j){
            H.assign(i, j, 0);
        }
    }
    double y_est;
    unsigned int data_idx = 0, dpi_offset = 0, dpi_idx = 0;
    bool is_NA;
    for (int i = 0; i < n; i++) {
        double x = (data[(i + dpi_offset) / 4] >> (((i + dpi_offset) % 4) * 2) ) & 0b11;
        is_NA = is_NA_uint8(x);
        x = predicated_assignment(is_NA, x, genotype_average);

        const std::vector<double>& patient_pnc = gwas->phenotype_and_covars.data[i];

        y_est = (beta_g + offset)[0] * x;
        for (int j = 1; j < num_dimensions; j++) {
            y_est += patient_pnc[j] * (beta_g + offset)[j];
        }
        y_est = 1 / (1 + modified_pade_approx_oblivious(-y_est));

        update_upperH_and_Grad(y_est, x, patient_pnc);

        data_idx++;
        int data_idx_lt_lengths = data_idx < dpi_lengths[dpi_idx];
        // if data_idx >= lengths, data_idx = 0 - otherwise multiply by 1
        data_idx *= data_idx_lt_lengths;
        // if data_idx >= lengths, increment dpi_offset, otherwise multiply by 0
        dpi_offset += (!data_idx_lt_lengths) * ((4 - ((1 + i + dpi_offset) % 4)) % 4);
    }
    /* build lower half of H */
    for (int j = 0; j < num_dimensions; j++) {
        for (int k = j + 1; k < num_dimensions; k++) {
            H.inner_assign(j, k, k, j);
        }
    }
}

//Good!
void Oblivious_log_row::update_upperH_and_Grad(double y_est, double x, const std::vector<double>& patient_pnc) {
    double y_est_1_y = y_est * (1 - y_est);
    double y_delta = patient_pnc[0] - y_est;
    (Grad_g + offset)[0] += y_delta * x;
    H.plus_equals(0, 0, x * x * y_est_1_y);
    for (int j = 1; j < num_dimensions; j++) {
        double patient_pnc_j = patient_pnc[j];
        double pnc_j_times_y_est = patient_pnc_j * y_est_1_y;
        (Grad_g + offset)[j] += y_delta * patient_pnc_j;
        H.plus_equals(j, 0, x * pnc_j_times_y_est);
        H.plus_equals(j, j, patient_pnc_j * pnc_j_times_y_est);

        for (int k = 1; k < j; k++) {
            H.plus_equals(j, k, patient_pnc[k] * pnc_j_times_y_est);
        }
    }
}

void Oblivious_log_row::get_outputs(int thread_id, std::string& output_string) {
    if (!fitted) {
        output_string += "\tNA\tNA\tNA";
        fitted = true;
        return;
    }
    output_string += "\t" + std::to_string((beta_g + offset)[0]) +
                     "\t" + std::to_string(standard_error) +
                     "\t" + std::to_string((beta_g + offset)[0] / standard_error);

}
