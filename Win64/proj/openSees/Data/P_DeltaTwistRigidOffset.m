clear;clc;
L76x51x5Lambda12Mid20ele=load('L76x51x5Lambda12Mid20ele.out');
L76x51x5Lambda12Mid20eleRigidOffset=load('L76x51x5Lambda12Mid20eleRigidOffset.out');
L76x51x5Lambda20Mid20ele=load('L76x51x5Lambda20Mid20ele.out');
L76x51x5Lambda20Mid20eleRigidOffset=load('L76x51x5Lambda20Mid20eleRigidOffset.out');
%% plot Pc/PY vs twist
figure
plot(L76x51x5Lambda12Mid20ele(11:end,3),L76x51x5Lambda12Mid20ele(11:end,1))
hold on
plot(L76x51x5Lambda12Mid20eleRigidOffset(11:end,3),L76x51x5Lambda12Mid20eleRigidOffset(11:end,1))
plot(L76x51x5Lambda20Mid20ele(11:end,3),L76x51x5Lambda20Mid20ele(11:end,1))
plot(L76x51x5Lambda20Mid20eleRigidOffset(11:end,3),L76x51x5Lambda20Mid20eleRigidOffset(11:end,1))
legend('Lambda1.2-DB20','Lambda1.2-DB20RigidOffset','Lambda2.0-DB20','Lambda2.0-DB20RigidOffset')
% xlabel('rad') 
% ylabel('Pc/PY')
% xlim([-0.2 0.2])
% ylim([0 1.6])
% set(gca,'YTick',(0:0.05:1.6))
% grid on