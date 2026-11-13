/*
 * Troca de idioma do site (PT-BR / EN).
 *
 * Como funciona: cada texto da pagina existe duas vezes no HTML, uma com
 * class="pt" e outra com class="en". O CSS mostra so uma das duas olhando o
 * atributo data-idioma do <html>:
 *   - sem data-idioma (o padrao, e o caso de quem esta sem JS): PT;
 *   - data-idioma="en": EN.
 * Este script so liga e desliga esse atributo, ajusta <html lang> (leitores
 * de tela e hifenizacao dependem dele) e troca o document.title, que nao
 * pode ter dois <span> como o resto do texto.
 *
 * A pagina SEMPRE abre em PT-BR, a nao ser que a pessoa ja tenha escolhido
 * EN numa visita anterior: a escolha fica no localStorage. Todo acesso ao
 * storage fica dentro de try/catch, porque aba privada, extensao ou politica
 * do navegador podem fazer o getItem/setItem lancar excecao; nesse caso a
 * escolha so nao e lembrada, e o resto continua funcionando.
 *
 * O script e carregado no <head> sem defer. A primeira parte (aplicar o
 * idioma salvo no <html>) roda na hora, antes da pagina ser pintada, para
 * quem escolheu EN nao ver o PT piscar. A segunda parte (os botoes) espera o
 * DOM existir.
 */
(function () {
  'use strict';

  var CHAVE = 'tcc-autotune-idioma';
  var TITULOS = {
    'pt-BR': 'TCC Autotune: correção de afinação vocal gratuita',
    'en': 'TCC Autotune: free vocal pitch correction'
  };
  var raiz = document.documentElement;

  function lerEscolha() {
    try {
      return window.localStorage.getItem(CHAVE);
    } catch (erro) {
      return null; // storage bloqueado: segue no padrao, PT-BR
    }
  }

  function guardarEscolha(idioma) {
    try {
      window.localStorage.setItem(CHAVE, idioma);
    } catch (erro) {
      // Storage bloqueado: a troca vale so ate recarregar. Nada a fazer.
    }
  }

  // Qualquer valor que nao seja 'en' (inclusive lixo no storage) vira PT-BR.
  function normalizar(idioma) {
    return idioma === 'en' ? 'en' : 'pt-BR';
  }

  function aplicar(idioma) {
    raiz.lang = idioma;
    if (idioma === 'en') {
      raiz.setAttribute('data-idioma', 'en');
    } else {
      raiz.removeAttribute('data-idioma');
    }
    document.title = TITULOS[idioma];

    var botoes = document.querySelectorAll('.idiomas button[data-idioma]');
    for (var i = 0; i < botoes.length; i++) {
      var ativo = botoes[i].getAttribute('data-idioma') === idioma;
      botoes[i].setAttribute('aria-pressed', ativo ? 'true' : 'false');
    }
  }

  // Parte 1, imediata: o <html> ja existe mesmo com o script no <head>.
  aplicar(normalizar(lerEscolha()));

  // Parte 2: liga os botoes e so entao mostra o grupo (que vem com hidden
  // no HTML, para nao aparecer um botao morto para quem esta sem JS).
  function ligarBotoes() {
    var grupo = document.querySelector('.idiomas');
    if (!grupo) return;

    grupo.addEventListener('click', function (evento) {
      var botao = evento.target.closest('button[data-idioma]');
      if (!botao) return;
      var idioma = normalizar(botao.getAttribute('data-idioma'));
      aplicar(idioma);
      guardarEscolha(idioma);
    });

    // Reaplica para acertar o aria-pressed agora que os botoes existem.
    aplicar(normalizar(raiz.getAttribute('data-idioma')));
    grupo.hidden = false;
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', ligarBotoes);
  } else {
    ligarBotoes();
  }
})();
