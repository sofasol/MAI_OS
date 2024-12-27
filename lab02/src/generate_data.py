# Простой скрипт для генерации файла с массивами чисел одинаковой длины

# Задаём количество массивов и размер каждого массива
K = 20000  # Количество массивов
M = 20000  # Размер каждого массива

# Имя выходного файла
output_file = 'data.txt'

# Открываем файл для записи
with open(output_file, 'w') as file:
    # Записываем первую строку с K и M
    file.write(f"{K} {M}\n")

    # Заполняем массивы последовательными числами
    current_number = 1
    for _ in range(K):
        array = []
        for _ in range(M):
            array.append(str(current_number))
            current_number += 1
        # Записываем массив в файл
        file.write(' '.join(array) + '\n')

print(f"Файл '{output_file}' успешно создан с {K} массивами по {M} элементов.")
