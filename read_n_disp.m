%% read file headers
% A = importdata('C:\Users\admin\Desktop\GageCodes\Liav\alpha_talk\Channel01.sig');
% 
% key_val = A.data;
%%key = A.textdata;

%% read data using fscanf => fast operation
% profile on

%%
close all

clear data
tic
% fileID = fopen('C:\Program Files (x86)\Gage\CompuScope\CompuScope C SDK\C Samples\GageMultipleRecord\Sample output.txt','r');
% fileID = fopen('C:\Program Files (x86)\Gage\CompuScope\CompuScope C SDK\C# Samples\GageMultipleRecords\bin\Release\GAGE_FILE_CH1-1.txt','r');
fileID = fopen('C:\Program Files (x86)\Gage\CompuScope\CompuScope C SDK\C Samples\GageMultipleRecord\MulRec_CH1-50.txt','r');

flag=0;
while flag ~= 2
    tline = fgetl(fileID);
    if tline(1:3)=='---'
        flag = flag+1;
    end
end
data = fscanf(fileID , '%f');

fclose(fileID); clear fileID;
toc

data_even = data(2:2:end);

figure;
plot(data(2:2:end))
ylim([-0.5,0.5])

figure;
plot(data(1:end));
ylim([-0.5,0.5])



figure; imagesc(x,y,data_mat.')

ax = gca;
ax.YTick = [1 : size(data_mat,2)]
%%
seg_count = 50;
Seg_size = (length(data_even)/(seg_count));
data_mat = reshape (data_even  , Seg_size ,seg_count);

figure; imagesc(data_mat.')
xlabel('Samples', 'fontsize', 15 );
ylabel('Segment number', 'fontsize', 15 );


figure;
for kk = 1:seg_count 
    plot(data_mat(:,kk));
    ylim([-0.6,0.6])
    pause(0.5)
end

%% correlation
clear corr_out
ref = data_mat(:,1);
win = 5;

for kk = 1:size(data_mat,2)-1
    
    for jj = 1: size(data_mat,1)-win
        y1 =  ref(jj:jj+win-1).* data_mat(jj:jj+win-1, kk+1);
        y2 = sum(y1);
        corr_out(jj,kk) = y2;
    end
    
end

corr_out2 = corr_out.';
y = [1,2,3,4];
x = 1:8160;
figure; imagesc(x,data_mat)
xlabel('Samples', 'fontsize', 15 );
ylabel('Segment number', 'fontsize', 15 );
ax = gca;
ax.YTick = [1 : size(data_mat,2)]


%% read data using readmatrix => slow operation
%%tic
%%data2 = readmatrix('C:\Program Files (x86)\Gage\CompuScope\CompuScope C SDK\C Samples\GageMultipleRecord\MulRec_CH1-2.txt');
%%toc
%% Simulation - remove
close all; clear all

seg_count=100;
T = 81e-6;
Fs = 100e6;
t = [0:1/Fs:T-(1/Fs)];
f_rf = 35e3;

x = 0.5*sin(2*pi*f_rf*t);
noise = randn(seg_count, length(x))*(max(x)/50);
data = noise + repmat(x,[seg_count,1]);

for kk = 1:seg_count 
    figure; 
    plot(t*1e6,data(kk,:));   
    grid on;
    ax = gca;
    ax.FontSize = 16; 
    ylim([-0.6,0.6]); xlim([0,t(end)]*1e6);
    xlabel('Time [\musec]', 'fontsize', 15 );
    ylabel('Signal [V]', 'fontsize', 15 );
    
    pause(0.4);
    F(kk) = getframe(gcf); 
    close;
end

movie_title= 'Liav6';
V = VideoWriter(['C:\Users\admin\Desktop\GageCodes\Liav\alpha_talk\', movie_title], 'MPEG-4');

% set frame rate
V.FrameRate = 20;
open(V);
writeVideo(V,F);
close(V);

%% Fourier Transform on recorded data
data_mat_clean = data_mat(:,2:end);

% time domain :
y1 = data_mat_clean(250,:);
y2 = data_mat_clean(1050,:);
figure; plot(y1,'-*'); hold on; plot(y2,'r');

% frequency domain :
Fs_fast = 100e6; % Hz
dT_slow = (2*size(data_mat_clean,1)*(1/Fs_fast));
Fs_slow = 1/dT_slow;

[~,y1_fft,ff] = my_fft_func(y1 , Fs_slow);

figure;
plot(ff , abs(y1_fft),'-*' )


%%%%%% 

[data_f_dsb,data_f_ssb,ff] = my_fft_mat_func(data_mat_clean.',Fs_slow);

figure; imagesc([1:size(data_f_ssb,2)], ff , 10*log10(abs(data_f_ssb)));

