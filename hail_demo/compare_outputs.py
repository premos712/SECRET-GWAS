import scipy.stats

hail_z_stats = []
SECRET_z_stats = []
with open('Hail_result.vcf', 'r') as f1:
    with open('SECRET_results.vcf', 'r') as f2:
        f1.readline()
        for line_hail, line_SECRET in zip(f1, f2):
            hail_z_stats.append(float(line_hail.split('\t')[4]))
            SECRET_z_stats.append(float(line_SECRET.split('\t')[4]))

diffs = [abs(v1 - v2) for v1, v2 in zip(hail_z_stats, SECRET_z_stats)]

print('Average difference in Z stat', round(sum(diffs) / len(diffs), 6))
print('Maximum difference in Z stat', round(max(diffs), 6))
