import numpy as np
import matplotlib.pyplot as plt
import sys

def read_bin_img(file_name, type = 'uint8'):
    width, height = np.fromfile(file_name, dtype = 'uint32', count = 2)
    print(width, height)

    if (type == 'uint8' or type == 'uint16' or type == 'uint32'):
        input_data = np.fromfile(file_name, dtype = type, offset = 8)
    else:
        raise TypeError('Type must be uint8, uint16 or uint32')

    input_data = np.reshape(input_data, (height, width))
    return input_data

def write_bin_img(file_name, img_out, type = 'uint8'):
    file_out = open(file_name, 'wb')
    height, width = np.shape(img_out)

    file_out.write(width.to_bytes(4, 'little'))
    file_out.write(height.to_bytes(4, 'little'))

    if (type == 'uint8' or type == 'uint16' or type == 'uint32'):
        file_out.write(img_out.flatten().astype(type))
    else:
        raise TypeError('Type must be uint8, uint16 or uint32')

    file_out.close()

if __name__ == '__main__':
    if len(sys.argv) > 1:
        file_name_ext = sys.argv[1]
    else:
        file_name_ext = 'lena.bin'

    file_name, _ = file_name_ext.rsplit('.', 1)
    destination_file = file_name + '.out'

    source_img = read_bin_img(file_name_ext)
    dest_img = read_bin_img(destination_file)

    fig = plt.figure(figsize = (24, 12), dpi = 80)
    src_sp = fig.add_subplot(1,2,1)
    dst_sp = fig.add_subplot(1,2,2)
    src_sp.imshow(source_img, cmap = 'gray')
    dst_sp.imshow(dest_img, cmap = 'gray')
    plt.show()