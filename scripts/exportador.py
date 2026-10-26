import csv

# Links físicos da Google Sycamore (53 qubits ativos)
sycamore_edges = [
    (0,1),(0,5),(1,2),(1,6),(2,3),(2,7),(3,4),(3,8),(4,9),
    (5,6),(5,10),(6,7),(6,11),(7,8),(7,12),(8,9),(8,13),(9,14),
    (10,11),(10,15),(11,12),(11,16),(12,13),(12,17),(13,14),(13,18),(14,19),
    (15,16),(15,20),(16,17),(16,21),(17,18),(17,22),(18,19),(18,23),(19,24),
    (20,21),(20,25),(21,22),(21,26),(22,23),(22,27),(23,24),(23,28),(24,29),
    (25,26),(25,30),(26,27),(26,31),(27,28),(27,32),(28,29),(28,33),(29,34),
    (30,31),(30,35),(31,32),(31,36),(32,33),(32,37),(33,34),(33,38),(34,39),
    (35,36),(35,40),(36,37),(36,41),(37,38),(37,42),(38,39),(38,43),(39,44),
    (40,41),(40,45),(41,42),(41,46),(42,43),(42,47),(43,44),(43,48),(44,49),
    (45,46),(45,50),(46,47),(46,51),(47,48),(47,52),(48,49),(50,51),(51,52)
]

def calcular_distancias_sabre(size, edges):
    dist = [[float('inf')] * size for _ in range(size)]
    for i in range(size):
        dist[i][i] = 0
    for u, v in edges:
        dist[u][v] = 1
        dist[v][u] = 1
    # Floyd-Warshall determinístico
    for k in range(size):
        for i in range(size):
            for j in range(size):
                if dist[i][j] > dist[i][k] + dist[k][j]:
                    dist[i][j] = dist[i][k] + dist[k][j]
    flat = []
    for i in range(size):
        for j in range(size):
            flat.append(int(dist[i][j]))
    return flat

# Processa a Sycamore
sycamore_flat = calcular_distancias_sabre(53, sycamore_edges)

# Gera o arquivo txt contendo a estrutura pura do array em C
with open("matrizes_verificacao.txt", "w") as f:
    f.write("// Copie este bloco direto para o seu codigo C/C++ de validacao\n")
    f.write(f"int SYCAMORE[2809] = {{\n    ")
    for idx, val in enumerate(sycamore_flat):
        if idx == len(sycamore_flat) - 1:
            f.write(f"{val}\n")
        elif (idx + 1) % 25 == 0:
            f.write(f"{val},\n    ")
        else:
            f.write(f"{val}, ")
    f.write("};\n")

print("Sucesso! O arquivo 'matrizes_verificacao.txt' contem o array 'int SYCAMORE[2809]' plano pronto para testes.")

