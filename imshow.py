import sys
import os
import numpy as np
import matplotlib.pyplot as plt
from PIL import Image

def read_bin_img(file_name, type = 'uint8'):
    width, height = np.fromfile(file_name, dtype = 'uint32', count = 2)
    print(width, height)

    if type == 'uint8' or type == 'uint16' or type == 'uint32':
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

    if type == 'uint8' or type == 'uint16' or type == 'uint32':
        file_out.write(img_out.flatten().astype(type))
    else:
        raise TypeError('Type must be uint8, uint16 or uint32')

    file_out.close()

if __name__ == '__main__':
    if len(sys.argv) > 1:
        file_name_ext = sys.argv[1]
        if not os.path.exists(file_name_ext):
            print(f'{file_name_ext} not fount')
            exit()
        
        source_img = read_bin_img(file_name_ext)
        
        file_name, _ = file_name_ext.rsplit('.', 1)
        destination_file = file_name + '.out'
        
        if os.path.exists(destination_file):
            dest_img = read_bin_img(destination_file)

            fig = plt.figure(figsize = (24, 12), dpi = 80)
            src_sp = fig.add_subplot(1,2,1)
            dst_sp = fig.add_subplot(1,2,2)
            src_sp.imshow(source_img, cmap = 'gray')
            dst_sp.imshow(dest_img, cmap = 'gray')
            plt.show()
        else:
            plt.figure(figsize = (12, 12), dpi = 80)
            plt.imshow(source_img, cmap = 'gray')
            plt.show()
    else:
        files = os.listdir('.')
        files = [file for file in files if file.endswith('.bin') or file.endswith('.out')]
        for file in files:
            img = read_bin_img(file)
            img = Image.fromarray(img, mode='L')
            file = file.replace('.','_') + '.png'
            img.save(file)

    