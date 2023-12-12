import re
import numpy as np

def read_c_array(file_path):
    """ 从 C 文件中读取 unsigned char 数组 """
    with open(file_path, 'r') as file:
        content = file.read()
        array_str = re.search(r'\{(.*?)\}', content, re.DOTALL).group(1)
        array_str = array_str.replace('\n', '').replace(' ', '').replace('0x', '')
        array_elements = array_str.split(',')
        array = np.array([int(element, 16) for element in array_elements if element != ''], dtype=np.uint8)
        return array

def combine_bytes_to_int16(array):
    """ 将 uint8 数组组合成 int16 数组 """
    return np.frombuffer(array, dtype=np.int16)

def adjust_volume(audio_data, volume_factor):
    """ 调整音量 """
    adjusted_data = np.clip(audio_data * volume_factor, -32768, 32767)
    return adjusted_data.astype(np.int16)

def split_int16_to_bytes(array):
    """ 将 int16 数组分解为 uint8 数组 """
    return np.frombuffer(array.tobytes(), dtype=np.uint8)

def save_c_array(file_path, array, array_name):
    """ 将数据保存回 C 数组 """
    formatted_array = ', '.join(f'0x{byte:02x}' for byte in array)
    with open(file_path, 'w') as file:
        file.write(f"const unsigned char {array_name}[] = {{\n{formatted_array}\n}};\n")

# 使用示例
input_file = 'main/audio_data.c'  # 输入的 C 文件路径
output_file = 'main/audio_output.c'  # 输出的 C 文件路径
volume_factor = 0.5  # 音量调整因子

audio_data = read_c_array(input_file)
print(len(audio_data))
if len(audio_data) % 2 != 0:
    raise ValueError("数组的长度必须是 2 的倍数")

audio_data = audio_data.view(np.int16)  # 以 16-bit 整数解释数据
adjusted_data = adjust_volume(audio_data, volume_factor)
save_c_array(output_file, adjusted_data, 'wav_unsigned_8bit_click_adjusted')
