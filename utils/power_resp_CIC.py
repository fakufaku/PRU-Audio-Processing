import numpy as np
import matplotlib.pyplot as plt
import sys

eps = sys.float_info.epsilon

def P_hat(N, M, R, fs):

    n_points = 1000
    freqs = np.linspace(0,(R / 2) * (1 / M),n_points)
    P_est = np.zeros((n_points,1))
    P = P_est

    for idx, f in enumerate(freqs):
        P_est[idx] = ( R*M * np.sin(np.pi*M*f) / (np.pi*M*f + eps) ) ** (2*N)
        P[idx] = (np.sin(np.pi*M*f)/(np.sin(np.pi*f/R)+eps))**(2*N)

    freqs_hz = freqs * fs / R

    # drop DC value since NaN due to division by 0
    return P[1:], P_est[1:], freqs_hz[1:]  

def bit_width(N, M, R):

    return np.ceil(1 + np.log2(R*M) * N)

if __name__ == "__main__":

    N = 3
    M = 1
    R = 16
    fs = 64 * 16e3
    P, P_est, freqs_hz = P_hat(N,M,R,fs)
    B = bit_width(N, M, R)

    # convert to dB and normalize so we get attenuation
    P_db = 10*np.log10(np.abs(P))
    P_db = P_db - P_db.max()

    P_est_db = 10*np.log10(np.abs(P_est))
    P_est_db = P_est_db - P_est_db.max()

    # find fc
    fc = freqs_hz[np.argmin(abs(P_db+3))]  # -3dB point

    fr = fs / R

    # visualize
    plt.ion()
    plt.figure()
    plt.plot(freqs_hz / fr, P_db,'b-')
    plt.axvline(x=fc / fr, color='r', linestyle='--')
    plt.axvline(x=8000 / fr, color='g', linestyle='--')
    # plt.plot(freqs_hz, P_est_db)   # P_db essentially same as P_dB_est

    for r in range(1, R//2):
        plt.axvline(x=r - 8000 / fr, color='r', linestyle='-')
        plt.axvline(x=r + 8000 / fr, color='r', linestyle='-')
    plt.axvline(x=R // 2 - 8000 / fr, color='r', linestyle='-')

    # add plot of f_c line

    plt.tight_layout()
    plt.title('N=%d, M=%d, R=%d, B=%d'%(N,M,R,B), fontsize=20)
    plt.grid()
    plt.xlabel('Folds', fontsize=20)
    plt.ylabel('Attenuation [dB]', fontsize=20)
    plt.xlim([0, R / 2])
    plt.ylim([-120,0])
    plt.legend(['P', 'Cutoff', '8kHz'], fontsize=20)

    plt.tight_layout(pad=0.1)

    plt.show(block=True)
